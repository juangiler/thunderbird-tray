/*
 * SPDX-FileCopyrightText: 2026 Juan Giler <contact@example.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

/**
 * The only window of the application.
 *
 * The application itself lives in the system tray, therefore the window is
 * hidden by default and only shown on demand (tray menu, command line or
 * when the platform has no system tray at all).
 */
Kirigami.ApplicationWindow {
    id: root

    visible: false
    width: 680
    height: 780
    minimumWidth: 480
    minimumHeight: 480
    title: qsTr("Thunderbird Tray")

    onClosing: function (close) {
        // Closing the window only hides it; the tray icon keeps running.
        if (AppBackend.minimizeToTray) {
            close.accepted = false;
            root.hide();
        }
    }

    pageStack.initialPage: Kirigami.ScrollablePage {
        ColumnLayout {
            spacing: Kirigami.Units.largeSpacing

            Kirigami.Heading {
                level: 1
                text: qsTr("Thunderbird Tray")
                Layout.fillWidth: true
            }

            Kirigami.InlineMessage {
                type: Kirigami.MessageType.Information
                visible: !AppBackend.profileFound
                text: qsTr("No Thunderbird profile was found. Start Thunderbird once, then use “Rescan”.")
                Layout.fillWidth: true
            }

            Kirigami.InlineMessage {
                type: Kirigami.MessageType.Warning
                visible: AppBackend.newMail
                text: AppBackend.statusText
                Layout.fillWidth: true
            }

            SettingsCard {}
        }
    }
}
