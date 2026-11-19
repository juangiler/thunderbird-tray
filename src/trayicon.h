/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Juan Giler <contact@example.org>
 */

#pragma once

#include <QAction>
#include <QColor>
#include <QIcon>
#include <QObject>
#include <QSystemTrayIcon>

class QMenu;
class QTimer;

/**
 * Native system tray icon with a badge and a blinking animation.
 *
 * The icon is composed at runtime: a themed Breeze envelope is used as the
 * base and the unread counter is painted on top of it as a badge. That keeps
 * the application free of binary resources and works with any icon theme that
 * provides the standard mail icons.
 *
 * The badge follows the desktop theme: its color is the KDE accent color
 * unless the user picked one, and the counter is white on dark themes and
 * black on light ones.
 */
class TrayIcon : public QObject
{
    Q_OBJECT

public:
    explicit TrayIcon(QObject *parent = nullptr);
    ~TrayIcon() override;

    /// Number of unread messages shown in the badge.
    void setUnreadCount(int count);
    int unreadCount() const;

    /// Enables or disables the animation.
    void setAnimationEnabled(bool enabled);
    bool isAnimationEnabled() const;

    /// Interval between two animation frames, in milliseconds.
    void setAnimationInterval(int milliseconds);
    int animationInterval() const;

    /// Tooltip shown when hovering the icon.
    void setToolTipText(const QString &text);

    /// The entries of the context menu that depend on the state.
    void setWindowHidden(bool hidden);
    void setSnoozeMinutes(int minutes);
    void setThunderbirdRunning(bool running);
    bool isThunderbirdRunning() const;

    /**
     * Color of the badge.
     *
     * An invalid color means that the accent color of the desktop theme is
     * used.
     */
    void setBadgeColor(const QColor &color);
    QColor badgeColor() const;

    /// Whether the badge follows the accent color of the desktop theme.
    void setBadgeColorFollowsAccent(bool follows);
    bool badgeColorFollowsAccent() const;

    /**
     * Color of the account that speaks for the badge.
     *
     * An invalid color means that no account has a color of its own and the
     * global badge color is used.
     */
    void setAccountBadgeColor(const QColor &color);
    QColor accountBadgeColor() const;

    /**
     * Color of the counter on the badge.
     *
     * An invalid color means that the color is chosen automatically: white
     * on dark themes, black on light ones.
     */
    void setBadgeTextColor(const QColor &color);
    QColor badgeTextColor() const;

    /// Font family of the counter; an empty family uses the default font.
    void setBadgeFontFamily(const QString &family);
    QString badgeFontFamily() const;

    /// Font size of the counter; 0 scales it with the icon.
    void setBadgeFontSize(int size);
    int badgeFontSize() const;

    /// The accent color of the current desktop theme.
    QColor themeAccentColor() const;

    /// The color the badge is painted with right now.
    QColor effectiveBadgeColor() const;

    /// True when the current desktop theme is a dark one.
    bool isDarkTheme() const;

    /// True when the platform offers a system tray at all.
    static bool isSystemTrayAvailable();

    /// Makes sure the icon is visible in the tray.
    void show();

    /// The composed icon, as it is shown in the tray.
    QIcon icon() const;

Q_SIGNALS:
    void openRequested();
    void checkMailRequested();
    void settingsRequested();
    void quitRequested();
    /// Emitted when the accent color or the theme brightness changed.
    void themeChanged();
    /// Asks to minimize the Thunderbird window.
    void hideWindowRequested();
    /// Asks to restore the Thunderbird window.
    void showWindowRequested();
    /// Asks to snooze the new mail notification for @p minutes.
    void snoozeRequested(int minutes);
    /// Emitted when the context menu is about to be shown.
    void menuAboutToShow();

private Q_SLOTS:
    void onActivated(QSystemTrayIcon::ActivationReason reason);
    void onAnimationTick();
    void onThemeRefresh();

private:
    QIcon composeIcon() const;
    void refreshTheme();
    void startAnimation();
    void stopAnimation();
    QFont badgeFont() const;
    QColor effectiveTextColor() const;
    static QIcon themedIcon(const QStringList &names);

    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    QTimer *m_timer = nullptr;
    QTimer *m_themeTimer = nullptr;

    QAction *m_hideWindowAction = nullptr;
    QAction *m_showWindowAction = nullptr;
    QMenu *m_snoozeMenu = nullptr;
    QAction *m_cancelSnoozeAction = nullptr;

    int m_unreadCount = 0;
    int m_animationInterval = 800;
    bool m_animationEnabled = true;
    bool m_thunderbirdRunning = false;

    QColor m_badgeColor;
    QColor m_badgeTextColor;
    QColor m_accountBadgeColor;
    QString m_badgeFontFamily;
    int m_badgeFontSize = 0;
    bool m_badgeColorFollowsAccent = true;

    // Theme state, refreshed periodically.
    QColor m_themeAccentColor;
    bool m_darkTheme = false;

    // Animation state.
    double m_phase = 0.0;
};
