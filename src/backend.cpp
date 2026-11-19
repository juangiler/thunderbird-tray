/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Juan Giler <contact@example.org>
 */

#include "backend.h"

#include "themecolors.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

namespace
{
constexpr auto kSettingsEnabledFolders = "Monitoring/enabledFolders";
constexpr auto kSettingsFoldersConfigured = "Monitoring/foldersConfigured";
constexpr auto kSettingsAccountColor = "Appearance/accountColor/";
constexpr auto kApplicationId = "io.github.juangiler.thunderbird-tray";

QString autostartFileName()
{
    return QString::fromLatin1(kApplicationId) + QStringLiteral(".desktop");
}
} // namespace

AppBackend::AppBackend(QObject *parent)
    : QObject(parent)
    , m_settings(new QSettings(this))
    , m_tray(new TrayIcon(this))
    , m_folderModel(new FolderListModel(this))
    , m_window(new ThunderbirdWindow(this))
{
    m_watcher = new ThunderbirdWatcher;
    m_watcher->moveToThread(&m_watchThread);
    connect(&m_watchThread, &QThread::finished, m_watcher, &QObject::deleteLater);
    connect(&m_watchThread, &QThread::started, m_watcher, &ThunderbirdWatcher::start);
    connect(m_watcher, &ThunderbirdWatcher::scanFinished, this, &AppBackend::onScanFinished);

    connect(m_tray, &TrayIcon::openRequested, this, &AppBackend::openThunderbird);
    connect(m_tray, &TrayIcon::checkMailRequested, this, &AppBackend::checkMailNow);
    connect(m_tray, &TrayIcon::settingsRequested, this, &AppBackend::openSettings);
    connect(m_tray, &TrayIcon::quitRequested, this, &AppBackend::quitApplication);
    connect(m_tray, &TrayIcon::themeChanged, this, &AppBackend::themeAccentColorChanged);
    connect(m_tray, &TrayIcon::hideWindowRequested, this, &AppBackend::hideThunderbirdWindow);
    connect(m_tray, &TrayIcon::showWindowRequested, this, &AppBackend::showThunderbirdWindow);
    connect(m_tray, &TrayIcon::snoozeRequested, this, &AppBackend::snooze);
    connect(m_tray, &TrayIcon::menuAboutToShow, this, [this]() {
        // The state of the Thunderbird window may have changed.
        m_window->refreshState();
    });

    connect(m_window, &ThunderbirdWindow::stateChanged, this, [this]() {
        m_tray->setWindowHidden(m_window->isHidden());
        Q_EMIT windowStateChanged();
    });
    connect(m_window, &ThunderbirdWindow::snoozeRequested, this, &AppBackend::snooze);

    applySettings();

    // The tray icon is the application's only permanent user interface.
    m_tray->show();

    m_thunderbirdRunning = isThunderbirdRunning();
    m_tray->setThunderbirdRunning(m_thunderbirdRunning);

    // Ask the window manager about the Thunderbird window.
    if (m_thunderbirdRunning) {
        m_window->refreshState();
    }

    m_watchThread.start();

    // Optionally start Thunderbird together with the tray companion.
    if (launchThunderbirdOnStart() && !m_thunderbirdRunning) {
        openThunderbird();
    }

    // Keep the "Thunderbird is running" indicator reasonably fresh.
    auto *processTimer = new QTimer(this);
    connect(processTimer, &QTimer::timeout, this, [this]() {
        const bool running = isThunderbirdRunning();
        if (running != m_thunderbirdRunning) {
            m_thunderbirdRunning = running;
            m_tray->setThunderbirdRunning(running);
            Q_EMIT thunderbirdRunningChanged();
            Q_EMIT statusChanged();
        }
    });
    processTimer->start(5000);

    // The end of a snooze is noticed here.
    auto *snoozeTimer = new QTimer(this);
    connect(snoozeTimer, &QTimer::timeout, this, [this]() {
        if (m_snoozeUntil.isValid() && !snoozed()) {
            m_snoozeUntil = QDateTime();
            updateAnimation();
            m_tray->setSnoozeMinutes(0);
            Q_EMIT snoozeChanged();
            Q_EMIT statusChanged();
        }
    });
    snoozeTimer->start(15000);
}

AppBackend::~AppBackend()
{
    // Ask the worker to stop and give it a moment to finish the scan that
    // is possibly in flight. A QThread must never be destroyed while it is
    // still running.
    m_watchThread.requestInterruption();
    m_watchThread.quit();
    if (!m_watchThread.wait(3000)) {
        m_watchThread.terminate();
        m_watchThread.wait(1000);
    }
}

int AppBackend::unreadCount() const
{
    return m_unreadCount;
}

int AppBackend::monitoredFolders() const
{
    return m_folderModel->monitoredCount();
}

int AppBackend::totalMessages() const
{
    return m_totalMessages;
}

bool AppBackend::newMail() const
{
    return m_newMail;
}

QString AppBackend::statusText() const
{
    if (!m_profileFound) {
        return tr("No Thunderbird profile detected");
    }
    if (snoozed()) {
        return tr("Notifications snoozed for %n minute(s)", "", snoozeRemainingMinutes());
    }
    if (m_newMail) {
        return tr("%n unread message(s) — new mail received", "", m_unreadCount);
    }
    if (m_unreadCount == 0) {
        return tr("No unread messages");
    }
    return tr("%n unread message(s)", "", m_unreadCount);
}

QString AppBackend::profilePath() const
{
    return m_profilePath;
}

QString AppBackend::profileName() const
{
    return m_profileName;
}

bool AppBackend::profileFound() const
{
    return m_profileFound;
}

QStringList AppBackend::warnings() const
{
    return m_warnings;
}

bool AppBackend::thunderbirdRunning() const
{
    return m_thunderbirdRunning;
}

bool AppBackend::snoozed() const
{
    return m_snoozeUntil.isValid() && m_snoozeUntil > QDateTime::currentDateTime();
}

int AppBackend::snoozeRemainingMinutes() const
{
    if (!snoozed()) {
        return 0;
    }
    const qint64 seconds = QDateTime::currentDateTime().secsTo(m_snoozeUntil);
    return static_cast<int>(qMax<qint64>(0, (seconds + 59) / 60));
}

bool AppBackend::windowHidden() const
{
    return m_window->isHidden();
}

int AppBackend::thunderbirdWindowCount() const
{
    return m_window->windowCount();
}

bool AppBackend::canHideWindow() const
{
    return ThunderbirdWindow::isSupported();
}

void AppBackend::snooze(int minutes)
{
    if (minutes <= 0) {
        m_snoozeUntil = QDateTime();
    } else {
        m_snoozeUntil = QDateTime::currentDateTime().addSecs(static_cast<qint64>(minutes) * 60);
        // The new mail hint is dropped right away.
        if (m_newMail) {
            m_newMail = false;
            Q_EMIT newMailChanged();
        }
    }

    updateAnimation();
    m_tray->setSnoozeMinutes(minutes);
    Q_EMIT snoozeChanged();
    Q_EMIT statusChanged();
}

void AppBackend::hideThunderbirdWindow()
{
    m_window->hideWindow();
}

void AppBackend::showThunderbirdWindow()
{
    m_window->showWindow();
}

void AppBackend::updateAnimation()
{
    // While the notification is snoozed the icon stays still.
    m_tray->setAnimationEnabled(animateIcon() && !snoozed());
}

bool AppBackend::launchAtBoot() const
{
    return m_settings->value(QStringLiteral("General/launchAtBoot"), false).toBool();
}

bool AppBackend::launchThunderbirdOnStart() const
{
    return m_settings->value(QStringLiteral("General/launchThunderbirdOnStart"), false).toBool();
}

bool AppBackend::minimizeToTray() const
{
    return m_settings->value(QStringLiteral("General/minimizeToTray"), true).toBool();
}

bool AppBackend::animateIcon() const
{
    return m_settings->value(QStringLiteral("Animation/enabled"), true).toBool();
}

int AppBackend::animationInterval() const
{
    return m_settings->value(QStringLiteral("Animation/intervalMs"), 800).toInt();
}

int AppBackend::pollInterval() const
{
    return m_settings->value(QStringLiteral("Monitoring/pollIntervalSeconds"), 10).toInt();
}

bool AppBackend::badgeColorFollowsAccent() const
{
    return m_settings->value(QStringLiteral("Appearance/badgeColorFollowsAccent"), true).toBool();
}

QString AppBackend::badgeColor() const
{
    return m_settings->value(QStringLiteral("Appearance/badgeColor")).toString();
}

QString AppBackend::badgeTextColor() const
{
    return m_settings->value(QStringLiteral("Appearance/badgeTextColor")).toString();
}

QString AppBackend::badgeFontFamily() const
{
    return m_settings->value(QStringLiteral("Appearance/badgeFontFamily")).toString();
}

int AppBackend::badgeFontSize() const
{
    return m_settings->value(QStringLiteral("Appearance/badgeFontSize"), 0).toInt();
}

QString AppBackend::themeAccentColor() const
{
    return m_tray->themeAccentColor().name();
}

bool AppBackend::darkTheme() const
{
    return m_tray->isDarkTheme();
}

QString AppBackend::currentBadgeColor() const
{
    return m_tray->effectiveBadgeColor().name();
}

QString AppBackend::badgeAccount() const
{
    // The account that speaks for the badge right now.
    QHash<QString, QColor> colors;
    for (auto it = m_accountUnread.constBegin(); it != m_accountUnread.constEnd(); ++it) {
        const QColor color(m_settings->value(QString::fromLatin1(kSettingsAccountColor) + it.key()).toString());
        if (color.isValid()) {
            colors.insert(it.key(), color);
        }
    }
    return ThemeColors::dominantAccount(m_accountUnread, colors);
}

QAbstractListModel *AppBackend::folders() const
{
    return m_folderModel;
}

void AppBackend::setLaunchAtBoot(bool enabled)
{
    if (launchAtBoot() == enabled) {
        return;
    }
    m_settings->setValue(QStringLiteral("General/launchAtBoot"), enabled);
    writeAutostartFile(enabled);
    Q_EMIT launchAtBootChanged();
}

void AppBackend::setLaunchThunderbirdOnStart(bool enabled)
{
    if (launchThunderbirdOnStart() == enabled) {
        return;
    }
    m_settings->setValue(QStringLiteral("General/launchThunderbirdOnStart"), enabled);
    Q_EMIT launchThunderbirdOnStartChanged();
}

void AppBackend::setMinimizeToTray(bool enabled)
{
    if (minimizeToTray() == enabled) {
        return;
    }
    m_settings->setValue(QStringLiteral("General/minimizeToTray"), enabled);
    Q_EMIT minimizeToTrayChanged();
}

void AppBackend::setAnimateIcon(bool enabled)
{
    if (animateIcon() == enabled) {
        return;
    }
    m_settings->setValue(QStringLiteral("Animation/enabled"), enabled);
    updateAnimation();
    Q_EMIT animateIconChanged();
}

void AppBackend::setAnimationInterval(int milliseconds)
{
    const int clamped = qBound(100, milliseconds, 10000);
    if (animationInterval() == clamped) {
        return;
    }
    m_settings->setValue(QStringLiteral("Animation/intervalMs"), clamped);
    m_tray->setAnimationInterval(clamped);
    Q_EMIT animationIntervalChanged();
}

void AppBackend::setPollInterval(int seconds)
{
    const int clamped = qBound(2, seconds, 3600);
    if (pollInterval() == clamped) {
        return;
    }
    m_settings->setValue(QStringLiteral("Monitoring/pollIntervalSeconds"), clamped);
    QMetaObject::invokeMethod(m_watcher, "setPollInterval", Qt::QueuedConnection, Q_ARG(int, clamped));
    Q_EMIT pollIntervalChanged();
}

QVariantList AppBackend::accounts() const
{
    QVariantList result;
    for (auto it = m_accountUnread.constBegin(); it != m_accountUnread.constEnd(); ++it) {
        QVariantMap account;
        account.insert(QStringLiteral("name"), it.key());
        account.insert(QStringLiteral("unread"), it.value());
        account.insert(QStringLiteral("color"), m_settings->value(
                          QString::fromLatin1(kSettingsAccountColor) + it.key()).toString());
        result.append(account);
    }
    return result;
}

void AppBackend::setAccountColor(const QString &account, const QString &color)
{
    const QString key = QString::fromLatin1(kSettingsAccountColor) + account;
    if (m_settings->value(key).toString() == color) {
        return;
    }
    m_settings->setValue(key, color);
    updateBadgeColor();
    Q_EMIT accountsChanged();
}

void AppBackend::updateBadgeColor()
{
    // Accounts with a color of their own take precedence over the global
    // badge color; the account with the most unread messages speaks for
    // all of them.
    QHash<QString, QColor> colors;
    for (auto it = m_accountUnread.constBegin(); it != m_accountUnread.constEnd(); ++it) {
        const QString value = m_settings->value(QString::fromLatin1(kSettingsAccountColor) + it.key()).toString();
        const QColor color(value);
        if (color.isValid()) {
            colors.insert(it.key(), color);
        }
    }

    // An invalid color means: no account has one, use the global color.
    m_tray->setAccountBadgeColor(ThemeColors::dominantAccountColor(m_accountUnread, colors, QColor()));
}

void AppBackend::setBadgeColorFollowsAccent(bool follows)
{
    if (badgeColorFollowsAccent() == follows) {
        return;
    }
    m_settings->setValue(QStringLiteral("Appearance/badgeColorFollowsAccent"), follows);
    m_tray->setBadgeColorFollowsAccent(follows);
    updateBadgeColor();
    Q_EMIT badgeColorFollowsAccentChanged();
}

void AppBackend::setBadgeColor(const QString &color)
{
    if (badgeColor() == color) {
        return;
    }
    m_settings->setValue(QStringLiteral("Appearance/badgeColor"), color);
    updateBadgeColor();
    Q_EMIT badgeColorChanged();
}

void AppBackend::setBadgeTextColor(const QString &color)
{
    if (badgeTextColor() == color) {
        return;
    }
    m_settings->setValue(QStringLiteral("Appearance/badgeTextColor"), color);
    m_tray->setBadgeTextColor(QColor(color));
    Q_EMIT badgeTextColorChanged();
}

void AppBackend::setBadgeFontFamily(const QString &family)
{
    if (badgeFontFamily() == family) {
        return;
    }
    m_settings->setValue(QStringLiteral("Appearance/badgeFontFamily"), family);
    m_tray->setBadgeFontFamily(family);
    Q_EMIT badgeFontFamilyChanged();
}

void AppBackend::setBadgeFontSize(int size)
{
    const int clamped = qBound(0, size, 200);
    if (badgeFontSize() == clamped) {
        return;
    }
    m_settings->setValue(QStringLiteral("Appearance/badgeFontSize"), clamped);
    m_tray->setBadgeFontSize(clamped);
    Q_EMIT badgeFontSizeChanged();
}

void AppBackend::applySettings()
{
    m_tray->setAnimationEnabled(animateIcon());
    m_tray->setAnimationInterval(animationInterval());    m_tray->setBadgeColorFollowsAccent(badgeColorFollowsAccent());
    m_tray->setBadgeColor(QColor(badgeColor()));
    m_tray->setBadgeTextColor(QColor(badgeTextColor()));    m_tray->setBadgeFontFamily(badgeFontFamily());
    m_tray->setBadgeFontSize(badgeFontSize());
    m_settings->setValue(QStringLiteral("General/launchAtBoot"), launchAtBoot());
    writeAutostartFile(launchAtBoot());
    QMetaObject::invokeMethod(m_watcher, "setPollInterval", Qt::QueuedConnection, Q_ARG(int, pollInterval()));
}

void AppBackend::openThunderbird()
{
    m_newMail = false;
    Q_EMIT newMailChanged();

    // A native installation always wins over a sandboxed one.
    const QString native = QStandardPaths::findExecutable(QStringLiteral("thunderbird"));
    if (!native.isEmpty()) {
        if (!QProcess::startDetached(native, {})) {
            reportLaunchFailure(native);
        }
        return;
    }

    QString appId = m_flatpakAppId;
    if (appId.isEmpty()) {
        appId = QStringLiteral("org.mozilla.thunderbird");
    }

    const QString flatpak = QStandardPaths::findExecutable(QStringLiteral("flatpak"));
    if (!flatpak.isEmpty()) {
        // Thunderbird itself raises the existing window, which is the only
        // Wayland safe way of focusing a foreign window.
        if (!QProcess::startDetached(flatpak, {QStringLiteral("run"), appId})) {
            reportLaunchFailure(flatpak);
        }
        return;
    }

    m_warnings = QStringList{tr("Neither a native Thunderbird installation nor Flatpak was found.")};
    Q_EMIT profileChanged();
}

void AppBackend::reportLaunchFailure(const QString &program)
{
    m_warnings = QStringList{tr("Thunderbird could not be started (%1).").arg(program)};
    Q_EMIT profileChanged();
}

void AppBackend::checkMailNow()
{
    // Thunderbird has no public API to force a server round trip, so the
    // best we can do is refresh the local counters immediately. When
    // Thunderbird is not running it is started, which checks mail on startup.
    QMetaObject::invokeMethod(m_watcher, "requestScan", Qt::QueuedConnection);

    if (!isThunderbirdRunning()) {
        openThunderbird();
    }
}

void AppBackend::openSettings()
{
    Q_EMIT showWindowRequested();
}

void AppBackend::quitApplication()
{
    Q_EMIT quitRequested();
}

void AppBackend::rescan()
{
    QMetaObject::invokeMethod(m_watcher, "requestScan", Qt::QueuedConnection);
}

void AppBackend::setFolderEnabled(const QString &folderId, bool enabled)
{
    if (m_folderModel->isFolderMonitored(folderId) == enabled) {
        return;
    }
    if (!m_folderModel->setFolderEnabled(folderId, enabled)) {
        return;
    }
    storeEnabledFolders();
    // The counters have to be recalculated right away: waiting for the
    // next poll would show a badge that no longer matches the selection.
    recalculate();
}

bool AppBackend::isFolderEnabled(const QString &folderId) const
{
    return m_folderModel->isFolderMonitored(folderId);
}

bool AppBackend::defaultEnabled(const TbFolder &folder) const
{
    if (folder.isVirtual) {
        return false;
    }
    return folder.isInbox;
}

void AppBackend::storeEnabledFolders()
{
    const QStringList ids = m_folderModel->monitoredFolderIds().values();
    m_settings->setValue(QString::fromLatin1(kSettingsEnabledFolders), ids);
    m_settings->setValue(QString::fromLatin1(kSettingsFoldersConfigured), true);
}

void AppBackend::recalculate()
{
    const FolderListModel::Totals totals = m_folderModel->totals();

    if (totals.unread != m_unreadCount || totals.total != m_totalMessages) {
        m_unreadCount = totals.unread;
        m_totalMessages = totals.total;
        Q_EMIT unreadCountChanged();
    }

    // The unread messages of every account, for the per account colors.
    QMap<QString, int> accountUnread;
    const int rows = m_folderModel->rowCount();
    for (int row = 0; row < rows; ++row) {
        const QModelIndex index = m_folderModel->index(row, 0);
        const QString id = index.data(FolderListModel::FolderIdRole).toString();
        if (!m_folderModel->isFolderMonitored(id)) {
            continue;
        }
        const QString account = index.data(FolderListModel::AccountRole).toString();
        accountUnread[account] += index.data(FolderListModel::UnreadRole).toInt();
    }
    if (accountUnread != m_accountUnread) {
        m_accountUnread = accountUnread;
        Q_EMIT accountsChanged();
    }

    if (m_hasBaseline && m_unreadCount > m_knownUnread && !snoozed()) {
        m_newMail = true;
        Q_EMIT newMailChanged();
    }
    m_knownUnread = m_unreadCount;
    m_hasBaseline = true;

    updateBadgeColor();
    updateTray();
    Q_EMIT statusChanged();
}

void AppBackend::updateTray()
{
    m_tray->setUnreadCount(m_unreadCount);
    m_tray->setToolTipText(statusText());
}

void AppBackend::writeAutostartFile(bool enabled) const
{
    const QString configLocation = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (configLocation.isEmpty()) {
        return;
    }
    const QString directory = configLocation + QStringLiteral("/autostart");
    const QString path = directory + QLatin1Char('/') + autostartFileName();

    if (!enabled) {
        QFile::remove(path);
        return;
    }

    if (!QDir().mkpath(directory)) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }

    const QString executable = QCoreApplication::applicationFilePath();
    const QString contents = QStringLiteral("[Desktop Entry]\n"
                                            "Type=Application\n"
                                            "Name=Thunderbird Tray\n"
                                            "Comment=System tray companion for Thunderbird\n"
                                            "Exec=\"%1\"\n"
                                            "Terminal=false\n"
                                            "X-GNOME-Autostart-enabled=true\n"
                                            "X-KDE-autostart-after=panel\n"
                                            "Categories=Network;Email;\n")
                                .arg(executable);

    if (file.write(contents.toUtf8()) != contents.toUtf8().size()) {
        return;
    }
}

void AppBackend::onScanFinished(const TbScanResult &result)
{
    m_profileFound = result.profileFound;
    m_profilePath = result.profilePath;
    m_profileName = result.profileName;
    m_warnings = result.warnings;
    m_flatpakAppId = result.flatpakAppId;

    // Restore the monitoring state before the model is filled.
    const bool configured = m_settings->value(QString::fromLatin1(kSettingsFoldersConfigured), false).toBool();
    QSet<QString> monitored;
    if (configured) {
        const QStringList stored = m_settings->value(QString::fromLatin1(kSettingsEnabledFolders)).toStringList();
        monitored = QSet<QString>(stored.cbegin(), stored.cend());
    } else {
        for (const TbFolder &folder : result.folders) {
            if (defaultEnabled(folder)) {
                monitored.insert(folder.id);
            }
        }
    }

    m_folderModel->setFolders(result.folders, monitored);

    recalculate();

    Q_EMIT profileChanged();
}

bool AppBackend::isThunderbirdRunning()
{
    const QStringList pids = QDir(QStringLiteral("/proc")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    static const QStringList knownNames = {
        QStringLiteral("thunderbird"),
        QStringLiteral("thunderbird-bin"),
        QStringLiteral("thunderbird-esr"),
        QStringLiteral("betterbird"),
    };

    for (const QString &pid : pids) {
        QFile comm(QStringLiteral("/proc/") + pid + QStringLiteral("/comm"));
        if (!comm.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QString name = QString::fromLatin1(comm.readAll()).trimmed();
        if (knownNames.contains(name, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}
