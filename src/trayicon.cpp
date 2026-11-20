/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Juan Giler <contact@example.org>
 */

#include "trayicon.h"

#include "themecolors.h"

#include <QAction>
#include <QFontMetrics>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QTimer>

#include <cmath>

namespace
{
/// Logical size the icon is rendered at before it is scaled by the tray.
constexpr int kRenderSize = 64;
/// Radius of the unread badge: a 30 px circle in a 64 px icon.
constexpr qreal kBadgeRadius = kRenderSize * (15.0 / 64.0);
/// Radius of the "Thunderbird is running" indicator.
constexpr qreal kRunningDotRadius = kRenderSize * 0.11;
/// The indicator is centred on the bottom-left corner of the envelope.
constexpr qreal kRunningDotX = kRenderSize * 0.11;
constexpr qreal kRunningDotY = kRenderSize * 0.80;
/// The badge fades between fully opaque and this alpha value.
constexpr int kMinimumBadgeAlpha = 60;
/// The animation is drawn with this many frames per second.
constexpr int kAnimationFrameMs = 33;
/// The theme is re-read with this interval, in milliseconds.
constexpr int kThemeRefreshMs = 10000;
} // namespace

TrayIcon::TrayIcon(QObject *parent)
    : QObject(parent)
    , m_tray(new QSystemTrayIcon(this))
    , m_menu(new QMenu)
    , m_timer(new QTimer(this))
    , m_themeTimer(new QTimer(this))
{
    m_menu->setObjectName(QStringLiteral("thunderbird-tray-menu"));

    auto *openAction = m_menu->addAction(QIcon::fromTheme(QStringLiteral("mail-message")), tr("Open Thunderbird"));
    connect(openAction, &QAction::triggered, this, &TrayIcon::openRequested);

    // Hiding and restoring the Thunderbird window.
    m_hideWindowAction = m_menu->addAction(QIcon::fromTheme(QStringLiteral("view-fullscreen")), tr("Hide Thunderbird Window"));
    connect(m_hideWindowAction, &QAction::triggered, this, &TrayIcon::hideWindowRequested);

    m_showWindowAction = m_menu->addAction(QIcon::fromTheme(QStringLiteral("window-restore")), tr("Show Thunderbird Window"));
    connect(m_showWindowAction, &QAction::triggered, this, &TrayIcon::showWindowRequested);

    m_menu->addSeparator();

    auto *checkAction = m_menu->addAction(QIcon::fromTheme(QStringLiteral("mail-message-new")), tr("Check Mail Now"));
    connect(checkAction, &QAction::triggered, this, &TrayIcon::checkMailRequested);

    // Snoozing the new mail notification.
    m_snoozeMenu = m_menu->addMenu(QIcon::fromTheme(QStringLiteral("view-time")), tr("Snooze Notifications"));
    for (const int minutes : {5, 15, 30, 60}) {
        QAction *action = m_snoozeMenu->addAction(tr("For %n minute(s)", "", minutes));
        action->setProperty("minutes", minutes);
        connect(action, &QAction::triggered, this, [this, action]() {
            Q_EMIT snoozeRequested(action->property("minutes").toInt());
        });
    }
    m_snoozeMenu->addSeparator();
    m_cancelSnoozeAction = m_snoozeMenu->addAction(tr("Cancel Snooze"));
    connect(m_cancelSnoozeAction, &QAction::triggered, this, [this]() {
        Q_EMIT snoozeRequested(0);
    });

    m_menu->addSeparator();

    auto *settingsAction = m_menu->addAction(QIcon::fromTheme(QStringLiteral("configure")), tr("Settings…"));
    connect(settingsAction, &QAction::triggered, this, &TrayIcon::settingsRequested);

    m_menu->addSeparator();

    auto *quitAction = m_menu->addAction(QIcon::fromTheme(QStringLiteral("application-exit")), tr("Quit"));
    connect(quitAction, &QAction::triggered, this, &TrayIcon::quitRequested);

    m_tray->setContextMenu(m_menu);
    m_tray->setToolTip(tr("Thunderbird Tray"));
    m_tray->setIcon(composeIcon());

    connect(m_menu, &QMenu::aboutToShow, this, &TrayIcon::menuAboutToShow);

    connect(m_tray, &QSystemTrayIcon::activated, this, &TrayIcon::onActivated);
    connect(m_timer, &QTimer::timeout, this, &TrayIcon::onAnimationTick);
    connect(m_themeTimer, &QTimer::timeout, this, &TrayIcon::onThemeRefresh);

    refreshTheme();
    m_themeTimer->start(kThemeRefreshMs);

    // Only one of the two window actions can be used at a time.
    setWindowHidden(false);
    setSnoozeMinutes(0);
}

TrayIcon::~TrayIcon()
{
    // The tray icon has to go first: it refers to the context menu.
    if (m_tray) {
        m_tray->setParent(nullptr);
        delete m_tray;
        m_tray = nullptr;
    }
    delete m_menu;
    m_menu = nullptr;
}

bool TrayIcon::isSystemTrayAvailable()
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void TrayIcon::show()
{
    m_tray->show();
}

QIcon TrayIcon::icon() const
{
    return composeIcon();
}

void TrayIcon::setUnreadCount(int count)
{
    if (m_unreadCount == count) {
        return;
    }
    m_unreadCount = count;
    m_tray->setIcon(composeIcon());

    if (count > 0 && m_animationEnabled) {
        startAnimation();
    } else {
        stopAnimation();
    }
}

int TrayIcon::unreadCount() const
{
    return m_unreadCount;
}

void TrayIcon::setAnimationEnabled(bool enabled)
{
    if (m_animationEnabled == enabled) {
        return;
    }
    m_animationEnabled = enabled;
    if (!enabled) {
        stopAnimation();
    } else if (m_unreadCount > 0) {
        startAnimation();
    }
}

bool TrayIcon::isAnimationEnabled() const
{
    return m_animationEnabled;
}

void TrayIcon::setAnimationInterval(int milliseconds)
{
    const int clamped = qBound(100, milliseconds, 10000);
    if (m_animationInterval == clamped) {
        return;
    }
    m_animationInterval = clamped;
    if (m_timer->isActive()) {
        startAnimation();
    }
}

int TrayIcon::animationInterval() const
{
    return m_animationInterval;
}

void TrayIcon::setToolTipText(const QString &text)
{
    m_tray->setToolTip(text);
}

void TrayIcon::setWindowHidden(bool hidden)
{
    m_hideWindowAction->setVisible(!hidden);
    m_showWindowAction->setVisible(hidden);
}

void TrayIcon::setSnoozeMinutes(int minutes)
{
    m_cancelSnoozeAction->setVisible(minutes > 0);
    for (QAction *action : m_snoozeMenu->actions()) {
        if (action == m_cancelSnoozeAction) {
            continue;
        }
        // The action of the current duration is not offered again.
        action->setVisible(action->property("minutes").toInt() != minutes);
    }
}

void TrayIcon::setThunderbirdRunning(bool running)
{
    m_hideWindowAction->setEnabled(running);
    m_showWindowAction->setEnabled(running);

    if (m_thunderbirdRunning == running) {
        return;
    }
    m_thunderbirdRunning = running;
    m_tray->setIcon(composeIcon());
}

bool TrayIcon::isThunderbirdRunning() const
{
    return m_thunderbirdRunning;
}

void TrayIcon::setBadgeColor(const QColor &color)
{
    if (m_badgeColor == color) {
        return;
    }
    m_badgeColor = color;
    m_tray->setIcon(composeIcon());
}

QColor TrayIcon::badgeColor() const
{
    return m_badgeColor;
}

void TrayIcon::setBadgeColorFollowsAccent(bool follows)
{
    if (m_badgeColorFollowsAccent == follows) {
        return;
    }
    m_badgeColorFollowsAccent = follows;
    m_tray->setIcon(composeIcon());
}

bool TrayIcon::badgeColorFollowsAccent() const
{
    return m_badgeColorFollowsAccent;
}

void TrayIcon::setAccountBadgeColor(const QColor &color)
{
    if (m_accountBadgeColor == color) {
        return;
    }
    m_accountBadgeColor = color;
    m_tray->setIcon(composeIcon());
}

QColor TrayIcon::accountBadgeColor() const
{
    return m_accountBadgeColor;
}

void TrayIcon::setBadgeTextColor(const QColor &color)
{
    if (m_badgeTextColor == color) {
        return;
    }
    m_badgeTextColor = color;
    m_tray->setIcon(composeIcon());
}

QColor TrayIcon::badgeTextColor() const
{
    return m_badgeTextColor;
}

void TrayIcon::setBadgeFontFamily(const QString &family)
{
    if (m_badgeFontFamily == family) {
        return;
    }
    m_badgeFontFamily = family;
    m_tray->setIcon(composeIcon());
}

QString TrayIcon::badgeFontFamily() const
{
    return m_badgeFontFamily;
}

void TrayIcon::setBadgeFontSize(int size)
{
    const int clamped = qBound(0, size, 200);
    if (m_badgeFontSize == clamped) {
        return;
    }
    m_badgeFontSize = clamped;
    m_tray->setIcon(composeIcon());
}

int TrayIcon::badgeFontSize() const
{
    return m_badgeFontSize;
}

QColor TrayIcon::themeAccentColor() const
{
    return m_themeAccentColor;
}

bool TrayIcon::isDarkTheme() const
{
    return m_darkTheme;
}

void TrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
    case QSystemTrayIcon::MiddleClick:
        Q_EMIT openRequested();
        break;
    default:
        break;
    }
}

void TrayIcon::onThemeRefresh()
{
    refreshTheme();
    m_tray->setIcon(composeIcon());
}

void TrayIcon::refreshTheme()
{
    const ThemeColors::Theme theme = ThemeColors::read();
    const QColor accent = theme.accentColor.isValid() ? theme.accentColor : ThemeColors::fallbackAccentColor();
    const bool dark = theme.darkTheme;

    if (accent == m_themeAccentColor && dark == m_darkTheme) {
        return;
    }

    m_themeAccentColor = accent;
    m_darkTheme = dark;
    Q_EMIT themeChanged();
}

void TrayIcon::startAnimation()
{
    if (m_timer->isActive()) {
        return;
    }
    m_phase = 0.0;
    m_timer->start(kAnimationFrameMs);
}

void TrayIcon::stopAnimation()
{
    if (!m_timer->isActive()) {
        return;
    }
    m_timer->stop();
    m_phase = 0.0;
    m_tray->setIcon(composeIcon());
}

void TrayIcon::onAnimationTick()
{
    // A smooth fade: the badge slowly becomes transparent and opaque again.
    // The animation interval is the time it takes for one half cycle.
    const double period = static_cast<double>(m_animationInterval) * 2.0;
    m_phase += 2.0 * M_PI * (static_cast<double>(kAnimationFrameMs) / period);
    if (m_phase > 2.0 * M_PI) {
        m_phase = std::fmod(m_phase, 2.0 * M_PI);
    }
    m_tray->setIcon(composeIcon());
}

QFont TrayIcon::badgeFont() const
{
    QFont font;
    if (!m_badgeFontFamily.isEmpty()) {
        font.setFamily(m_badgeFontFamily);
    }
    font.setBold(true);
    font.setPixelSize(m_badgeFontSize > 0 ? m_badgeFontSize : static_cast<int>(kRenderSize * 0.36));
    return font;
}

QColor TrayIcon::effectiveBadgeColor() const
{
    // An account with a color of its own speaks for the badge.
    if (m_accountBadgeColor.isValid()) {
        return m_accountBadgeColor;
    }
    if (!m_badgeColorFollowsAccent && m_badgeColor.isValid()) {
        return m_badgeColor;
    }
    if (m_themeAccentColor.isValid()) {
        return m_themeAccentColor;
    }
    return ThemeColors::fallbackAccentColor();
}
QColor TrayIcon::effectiveTextColor() const
{
    return ThemeColors::counterColor(m_badgeTextColor, m_darkTheme);
}

QIcon TrayIcon::themedIcon(const QStringList &names)
{
    for (const QString &name : names) {
        const QIcon icon = QIcon::fromTheme(name);
        if (!icon.isNull()) {
            return icon;
        }
    }
    return QIcon();
}

QIcon TrayIcon::composeIcon() const
{
    const bool hasUnread = m_unreadCount > 0;

    QIcon base;
    if (hasUnread) {
        base = themedIcon({QStringLiteral("mail-unread"),
                           QStringLiteral("mail-unread-new"),
                           QStringLiteral("mail-message-new"),
                           QStringLiteral("mail-message")});
    } else {
        base = themedIcon({QStringLiteral("mail-read"),
                           QStringLiteral("mail-message"),
                           QStringLiteral("mail-unread")});
    }
    if (base.isNull()) {
        base = QIcon(QStringLiteral(":/resources/icons/thunderbird-tray.svg"));
    }

    QPixmap pixmap(kRenderSize, kRenderSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawPixmap(0, 0, base.pixmap(QSize(kRenderSize, kRenderSize)));

    // A dot centred on the bottom-left corner of the envelope reports the
    // state of Thunderbird: green while it is running, red when it is not.
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_thunderbirdRunning ? QColor(46, 204, 113)  // green
                                          : QColor(231, 76, 60));  // red
    painter.drawEllipse(QPointF(kRunningDotX, kRunningDotY), kRunningDotRadius, kRunningDotRadius);

    if (hasUnread) {
        // The 1 px inset puts the centre of the badge at x = 48 px.
        const QPointF centre(kRenderSize - kBadgeRadius - 1.0, kBadgeRadius + 1.0);

        QColor badge = effectiveBadgeColor();
        if (m_animationEnabled && m_timer->isActive()) {
            // The badge fades smoothly between opaque and transparent.
            const double wave = 0.5 + 0.5 * std::cos(m_phase);
            badge.setAlphaF(kMinimumBadgeAlpha / 255.0 + (1.0 - kMinimumBadgeAlpha / 255.0) * wave);
        }

        painter.setPen(Qt::NoPen);
        painter.setBrush(badge);
        painter.drawEllipse(centre, kBadgeRadius, kBadgeRadius);

        const QString text = m_unreadCount > 99 ? QStringLiteral("99+") : QString::number(m_unreadCount);
        QFont font = badgeFont();

        // Long counters (and large user-configured fonts) must not spill out
        // of the badge or get clipped by the 64 px icon. Shrink until the
        // glyph box fits inside the circle with a small margin.
        const qreal maxHalfDiagonal = kBadgeRadius - 1.0;
        while (font.pixelSize() > 6) {
            const QRectF candidate = QFontMetricsF(font).tightBoundingRect(text);
            if (std::hypot(candidate.width() / 2.0, candidate.height() / 2.0) <= maxHalfDiagonal) {
                break;
            }
            font.setPixelSize(font.pixelSize() - 1);
        }

        painter.setPen(effectiveTextColor());
        painter.setFont(font);

        // Qt::AlignVCenter centres the whole line box (including the font's
        // descent), which makes digits sit slightly below the badge centre.
        // Align the glyph bounds themselves instead, for both axes.
        const QRectF glyphs = QFontMetricsF(font).tightBoundingRect(text);
        painter.drawText(QPointF(centre.x() - glyphs.center().x(),
                                 centre.y() - glyphs.center().y()),
                         text);
    }

    painter.end();
    return QIcon(pixmap);
}
