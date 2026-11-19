/*
 * SPDX-FileCopyrightText: 2026 Juan Giler <contact@example.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

/**
 * All settings of the application, grouped into FormCards.
 *
 * FormCard and its delegates live in the separate QML module
 * "org.kde.kirigamiaddons.formcard" since Kirigami 6.
 */
ColumnLayout {
    id: root

    spacing: Kirigami.Units.largeSpacing

    // ----------------------------------------------------------------- general
    FormCard.FormHeader {
        title: qsTr("General")
    }

    FormCard.FormCard {
        FormCard.FormSwitchDelegate {
            text: qsTr("Start automatically at login")
            description: qsTr("Launches Thunderbird Tray when you log in.")
            checked: AppBackend.launchAtBoot
            onToggled: AppBackend.launchAtBoot = checked
        }

        FormCard.FormSwitchDelegate {
            text: qsTr("Launch Thunderbird when Thunderbird Tray starts")
            description: qsTr("Starts Thunderbird together with this tray companion.")
            checked: AppBackend.launchThunderbirdOnStart
            onToggled: AppBackend.launchThunderbirdOnStart = checked
        }

        FormCard.FormSwitchDelegate {
            text: qsTr("Minimize to tray instead of closing")
            description: qsTr("Closing this window keeps Thunderbird Tray running in the background.")
            checked: AppBackend.minimizeToTray
            onToggled: AppBackend.minimizeToTray = checked
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            text: qsTr("Open Thunderbird")
            description: AppBackend.thunderbirdRunning ? qsTr("Thunderbird is running.") : qsTr("Thunderbird is not running.")
            icon.name: "mail-message"
            onClicked: AppBackend.openThunderbird()
        }

        FormCard.FormButtonDelegate {
            text: AppBackend.windowHidden ? qsTr("Show Thunderbird window") : qsTr("Hide Thunderbird window")
            description: AppBackend.canHideWindow
                         ? qsTr("Minimizes the Thunderbird window, or brings it back.")
                         : qsTr("This needs the KWin window manager.")
            icon.name: AppBackend.windowHidden ? "window-restore" : "view-fullscreen"
            enabled: AppBackend.canHideWindow && AppBackend.thunderbirdRunning
            onClicked: {
                if (AppBackend.windowHidden)
                    AppBackend.showThunderbirdWindow()
                else
                    AppBackend.hideThunderbirdWindow()
            }
        }

        FormCard.FormButtonDelegate {
            text: qsTr("Snooze notifications")
            description: AppBackend.snoozed
                         ? qsTr("Snoozed for %n more minute(s).", "", AppBackend.snoozeRemainingMinutes)
                         : qsTr("The icon stays still and no new mail is announced.")
            icon.name: "view-time"
            onClicked: AppBackend.snooze(30)
        }

        FormCard.FormButtonDelegate {
            text: qsTr("Cancel snooze")
            description: qsTr("Ends the snooze right away.")
            icon.name: "view-refresh"
            visible: AppBackend.snoozed
            onClicked: AppBackend.snooze(0)
        }

        FormCard.FormButtonDelegate {
            text: qsTr("Check Mail Now")
            description: qsTr("Refreshes the unread counters immediately.")
            icon.name: "view-refresh"
            onClicked: AppBackend.checkMailNow()
        }

        FormCard.FormButtonDelegate {
            text: qsTr("Rescan profiles")
            description: qsTr("Looks for Thunderbird profiles again.")
            icon.name: "view-refresh"
            onClicked: AppBackend.rescan()
        }

        FormCard.FormTextDelegate {
            text: qsTr("Profile")
            description: AppBackend.profileFound ? AppBackend.profilePath : qsTr("Not found")
        }
    }

    // --------------------------------------------------------------- animation
    FormCard.FormHeader {
        title: qsTr("Animation")
    }

    FormCard.FormCard {
        FormCard.FormSwitchDelegate {
            text: qsTr("Animate the tray icon")
            description: qsTr("Flashes the tray icon while unread messages are present.")
            checked: AppBackend.animateIcon
            onToggled: AppBackend.animateIcon = checked
        }

        FormCard.FormSpinBoxDelegate {
            label: qsTr("Blink interval")
            description: qsTr("Milliseconds it takes the badge to fade in or out.")
            from: 100
            to: 5000
            stepSize: 50
            value: AppBackend.animationInterval
            onValueChanged: {
                if (value !== AppBackend.animationInterval)
                    AppBackend.animationInterval = value
            }
        }

        FormCard.FormSpinBoxDelegate {
            label: qsTr("Poll interval")
            description: qsTr("Seconds between two scans of the Thunderbird profile.")
            from: 2
            to: 300
            stepSize: 1
            value: AppBackend.pollInterval
            onValueChanged: {
                if (value !== AppBackend.pollInterval)
                    AppBackend.pollInterval = value
            }
        }
    }

    // -------------------------------------------------------------- appearance
    FormCard.FormHeader {
        title: qsTr("Appearance")
    }

    FormCard.FormCard {
        FormCard.FormSwitchDelegate {
            text: qsTr("Follow the system accent color")
            description: qsTr("The badge uses the accent color of the desktop theme (%1).")
                         .arg(AppBackend.themeAccentColor)
            checked: AppBackend.badgeColorFollowsAccent
            onToggled: AppBackend.badgeColorFollowsAccent = checked
        }

        FormCard.FormColorDelegate {
            id: badgeColorDelegate
            readonly property string effectiveColor: AppBackend.badgeColorFollowsAccent
                                                    ? AppBackend.themeAccentColor
                                                    : AppBackend.badgeColor
            text: qsTr("Badge color")
            color: effectiveColor
            enabled: !AppBackend.badgeColorFollowsAccent
            onColorChanged: {
                // Only a value that differs from the effective one comes
                // from the colour dialog; the binding does not trigger this.
                if (color.toString() !== effectiveColor)
                    AppBackend.badgeColor = color.toString()
            }
        }

        FormCard.FormSwitchDelegate {
            text: qsTr("Automatic counter color")
            description: AppBackend.darkTheme
                         ? qsTr("The counter is white, because the theme is dark.")
                         : qsTr("The counter is black, because the theme is light.")
            checked: AppBackend.badgeTextColor === ""
            onToggled: AppBackend.badgeTextColor = checked ? "" : "#ffffff"
        }

        FormCard.FormColorDelegate {
            id: counterColorDelegate
            readonly property string effectiveColor: AppBackend.badgeTextColor === ""
                                                    ? (AppBackend.darkTheme ? "#ffffff" : "#000000")
                                                    : AppBackend.badgeTextColor
            text: qsTr("Counter color")
            color: effectiveColor
            enabled: AppBackend.badgeTextColor !== ""
            onColorChanged: {
                if (color.toString() !== effectiveColor)
                    AppBackend.badgeTextColor = color.toString()
            }
        }

        FormCard.FormTextFieldDelegate {
            label: qsTr("Counter font")
            description: qsTr("The font family of the number on the badge. Leave empty for the system font.")
            placeholderText: qsTr("System font")
            text: AppBackend.badgeFontFamily
            onTextChanged: AppBackend.badgeFontFamily = text
        }

        FormCard.FormSpinBoxDelegate {
            label: qsTr("Counter font size")
            description: qsTr("The size of the number on the badge in pixels. 0 scales it with the icon.")
            from: 0
            to: 60
            stepSize: 1
            value: AppBackend.badgeFontSize
            onValueChanged: {
                if (value !== AppBackend.badgeFontSize)
                    AppBackend.badgeFontSize = value
            }
        }
    }

    // --------------------------------------------------------- account colors
    FormCard.FormHeader {
        title: qsTr("Account colors")
    }

    FormCard.FormCard {
        FormCard.FormSectionText {
            text: qsTr("There is only one badge, so the account with the most unread messages decides its color. Accounts without a color of their own use the color above.")
            visible: AppBackend.accounts.length > 0
        }

        Repeater {
            model: AppBackend.accounts

            delegate: FormCard.FormColorDelegate {
                required property var modelData

                readonly property string accountName: modelData.name
                readonly property string effectiveColor: modelData.color !== ""
                                                    ? modelData.color
                                                    : AppBackend.themeAccentColor

                text: accountName + " · " + qsTr("%1 unread").arg(modelData.unread)
                      + (accountName === AppBackend.badgeAccount
                         ? " · " + qsTr("decides the badge color")
                         : "")
                color: effectiveColor
                onColorChanged: {
                    if (color.toString() !== effectiveColor)
                        AppBackend.setAccountColor(accountName, color.toString())
                }
            }
        }
    }

    // ---------------------------------------------------------------- accounts
    FormCard.FormHeader {
        title: qsTr("Monitored folders")
    }
    FormCard.FormCard {
        FormCard.FormSectionText {
            text: AppBackend.folders.folderCount > 0
                  ? qsTr("%1 folders in %2 accounts. Uncheck a folder to stop counting it.")
                    .arg(AppBackend.folders.folderCount)
                    .arg(AppBackend.folders.accountCount)
                  : qsTr("No folders were detected yet.")
        }

        Repeater {
            id: folderRepeater

            model: AppBackend.folders

            delegate: FormCard.FormCheckDelegate {
                required property string account
                required property string folder
                required property int unread
                required property int total
                required property string folderId
                required property bool monitored

                text: folder
                description: account + " · " + qsTr("%1 unread of %2").arg(unread).arg(total)
                checked: monitored
                onToggled: AppBackend.setFolderEnabled(folderId, checked)
            }
        }
    }
}
