/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Juan Giler <contact@example.org>
 */

#pragma once

#include <QObject>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QThread>

#include "foldermodel.h"
#include "tbwatcher.h"
#include "trayicon.h"
#include "thunderbirdwindow.h"

class QSettings;

/**
 * Application backend: owns the settings, the tray icon and the background
 * watcher and exposes everything the QML layer needs.
 */
class AppBackend : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int unreadCount READ unreadCount NOTIFY unreadCountChanged)
    Q_PROPERTY(int monitoredFolders READ monitoredFolders NOTIFY unreadCountChanged)
    Q_PROPERTY(int totalMessages READ totalMessages NOTIFY unreadCountChanged)
    Q_PROPERTY(bool newMail READ newMail NOTIFY newMailChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(QString profilePath READ profilePath NOTIFY profileChanged)
    Q_PROPERTY(QString profileName READ profileName NOTIFY profileChanged)
    Q_PROPERTY(bool profileFound READ profileFound NOTIFY profileChanged)
    Q_PROPERTY(QStringList warnings READ warnings NOTIFY profileChanged)
    Q_PROPERTY(bool thunderbirdRunning READ thunderbirdRunning NOTIFY thunderbirdRunningChanged)
    Q_PROPERTY(bool snoozed READ snoozed NOTIFY snoozeChanged)
    Q_PROPERTY(int snoozeRemainingMinutes READ snoozeRemainingMinutes NOTIFY snoozeChanged)
    Q_PROPERTY(bool windowHidden READ windowHidden NOTIFY windowStateChanged)
    Q_PROPERTY(bool canHideWindow READ canHideWindow CONSTANT)
    Q_PROPERTY(bool launchAtBoot READ launchAtBoot WRITE setLaunchAtBoot NOTIFY launchAtBootChanged)
    Q_PROPERTY(bool launchThunderbirdOnStart READ launchThunderbirdOnStart WRITE setLaunchThunderbirdOnStart NOTIFY launchThunderbirdOnStartChanged)
    Q_PROPERTY(bool minimizeToTray READ minimizeToTray WRITE setMinimizeToTray NOTIFY minimizeToTrayChanged)
    Q_PROPERTY(bool animateIcon READ animateIcon WRITE setAnimateIcon NOTIFY animateIconChanged)
    Q_PROPERTY(int animationInterval READ animationInterval WRITE setAnimationInterval NOTIFY animationIntervalChanged)
    Q_PROPERTY(int pollInterval READ pollInterval WRITE setPollInterval NOTIFY pollIntervalChanged)
    Q_PROPERTY(bool badgeColorFollowsAccent READ badgeColorFollowsAccent WRITE setBadgeColorFollowsAccent NOTIFY badgeColorFollowsAccentChanged)
    Q_PROPERTY(QString badgeColor READ badgeColor WRITE setBadgeColor NOTIFY badgeColorChanged)
    Q_PROPERTY(QString badgeTextColor READ badgeTextColor WRITE setBadgeTextColor NOTIFY badgeTextColorChanged)
    Q_PROPERTY(QString badgeFontFamily READ badgeFontFamily WRITE setBadgeFontFamily NOTIFY badgeFontFamilyChanged)
    Q_PROPERTY(int badgeFontSize READ badgeFontSize WRITE setBadgeFontSize NOTIFY badgeFontSizeChanged)
    Q_PROPERTY(QString themeAccentColor READ themeAccentColor NOTIFY themeAccentColorChanged)
    Q_PROPERTY(bool darkTheme READ darkTheme NOTIFY themeAccentColorChanged)
    Q_PROPERTY(QString currentBadgeColor READ currentBadgeColor NOTIFY badgeColorChanged)
    Q_PROPERTY(QString badgeAccount READ badgeAccount NOTIFY accountsChanged)
    Q_PROPERTY(QVariantList accounts READ accounts NOTIFY accountsChanged)
    Q_PROPERTY(QAbstractListModel *folders READ folders CONSTANT)

public:
    explicit AppBackend(QObject *parent = nullptr);
    ~AppBackend() override;

    int unreadCount() const;
    int monitoredFolders() const;
    int totalMessages() const;
    bool newMail() const;
    QString statusText() const;
    QString profilePath() const;
    QString profileName() const;
    bool profileFound() const;
    QStringList warnings() const;
    bool thunderbirdRunning() const;
    bool snoozed() const;
    int snoozeRemainingMinutes() const;
    bool windowHidden() const;
    int thunderbirdWindowCount() const;
    bool canHideWindow() const;
    bool launchAtBoot() const;
    bool launchThunderbirdOnStart() const;
    bool minimizeToTray() const;
    bool animateIcon() const;
    int animationInterval() const;
    int pollInterval() const;
    bool badgeColorFollowsAccent() const;
    QString badgeColor() const;
    QString badgeTextColor() const;
    QString badgeFontFamily() const;
    int badgeFontSize() const;
    QString themeAccentColor() const;
    bool darkTheme() const;
    QString currentBadgeColor() const;
    QString badgeAccount() const;
    QVariantList accounts() const;
    QAbstractListModel *folders() const;

    void setLaunchAtBoot(bool enabled);
    void setLaunchThunderbirdOnStart(bool enabled);
    void setMinimizeToTray(bool enabled);
    void setAnimateIcon(bool enabled);
    void setAnimationInterval(int milliseconds);
    void setPollInterval(int seconds);
    void setBadgeColorFollowsAccent(bool follows);
    void setBadgeColor(const QString &color);
    void setBadgeTextColor(const QString &color);
    void setBadgeFontFamily(const QString &family);
    void setBadgeFontSize(int size);

public Q_SLOTS:
    /// Starts or focuses Thunderbird.
    void openThunderbird();
    /// Triggers an immediate refresh of the unread counters.
    void checkMailNow();
    /// Requests the settings window to be shown.
    void openSettings();
    /// Requests the application to terminate.
    void quitApplication();
    /// Forces a new profile scan.
    void rescan();
    /// Enables or disables monitoring of a single folder.
    void setFolderEnabled(const QString &folderId, bool enabled);
    /// Returns whether the given folder is monitored.
    bool isFolderEnabled(const QString &folderId) const;
    /// Sets the color of the badge for a single account.
    void setAccountColor(const QString &account, const QString &color);
    /// Snoozes the new mail notification; 0 minutes cancels the snooze.
    void snooze(int minutes);
    /// Hides the Thunderbird window.
    void hideThunderbirdWindow();
    /// Restores the Thunderbird window.
    void showThunderbirdWindow();

Q_SIGNALS:
    void unreadCountChanged();
    void newMailChanged();
    void statusChanged();
    void profileChanged();
    void thunderbirdRunningChanged();
    void snoozeChanged();
    void windowStateChanged();
    void launchAtBootChanged();
    void launchThunderbirdOnStartChanged();
    void minimizeToTrayChanged();
    void animateIconChanged();
    void animationIntervalChanged();
    void pollIntervalChanged();
    void badgeColorFollowsAccentChanged();
    void badgeColorChanged();
    void badgeTextColorChanged();
    void badgeFontFamilyChanged();
    void badgeFontSizeChanged();
    void themeAccentColorChanged();
    void accountsChanged();
    void showWindowRequested();
    void quitRequested();

private Q_SLOTS:
    void onScanFinished(const TbScanResult &result);

private:
    void applySettings();
    void recalculate();
    void updateAnimation();
    void updateTray();
    void updateBadgeColor();
    void writeAutostartFile(bool enabled) const;
    void storeEnabledFolders();
    void reportLaunchFailure(const QString &program);
    bool defaultEnabled(const TbFolder &folder) const;
    static bool isThunderbirdRunning();

    QSettings *m_settings = nullptr;
    TrayIcon *m_tray = nullptr;
    FolderListModel *m_folderModel = nullptr;
    ThunderbirdWindow *m_window = nullptr;

    QThread m_watchThread;
    ThunderbirdWatcher *m_watcher = nullptr;

    int m_unreadCount = 0;
    int m_totalMessages = 0;
    bool m_newMail = false;
    bool m_thunderbirdRunning = false;
    int m_knownUnread = 0;
    bool m_hasBaseline = false;

    QString m_profilePath;
    QString m_profileName;
    bool m_profileFound = false;
    QStringList m_warnings;
    QString m_flatpakAppId;

    // Unread messages of every monitored account.
    QMap<QString, int> m_accountUnread;

    // The new mail notification is snoozed until this moment.
    QDateTime m_snoozeUntil;
};
