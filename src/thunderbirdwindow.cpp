/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Juan Giler <contact@example.org>
 */

#include "thunderbirdwindow.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTextStream>

namespace
{
/// The name this application uses on the session bus.
constexpr auto kServiceName = "io.github.juangiler.thunderbird-tray";
/// The object the script of the desktop calls back into.
constexpr auto kObjectPath = "/ThunderbirdTray";
/// The interface of that object.
constexpr auto kInterfaceName = "io.github.juangiler.ThunderbirdTray";
/// The name under which the script is registered with the desktop.
constexpr auto kScriptName = "thunderbird-tray-window";

/**
 * The part of the script that finds the Thunderbird window.
 *
 * It is shared by all three actions, only the action itself differs.
 */
const char *kScriptPrologue = R"(
function isThunderbird(window) {
    if (window.specialWindow || window.dock) {
        return false;
    }
    var resourceClass = String(window.resourceClass || "").toLowerCase();
    var resourceName = String(window.resourceName || "").toLowerCase();
    if (resourceClass.indexOf("thunderbird") >= 0) {
        return true;
    }
    if (resourceName.indexOf("thunderbird") >= 0) {
        return true;
    }
    // The reader window of Thunderbird is called "<subject> - Thunderbird".
    var caption = String(window.caption || "");
    return caption.length > 0 && caption.substring(caption.length - 11) === " - Thunderbird";
}

function report(action, count, hidden) {
    callDBus("io.github.juangiler.thunderbird-tray", "/ThunderbirdTray",
             "io.github.juangiler.ThunderbirdTray", "windowAction", action, count, hidden);
}
)";

/// Reports back how many windows were found and how many are minimized.
const char *kHideScript = R"(
function act() {
    var clients = workspace.windowList();
    var count = 0;
    for (var i = 0; i < clients.length; ++i) {
        if (!isThunderbird(clients[i])) {
            continue;
        }
        count++;
        clients[i].minimized = true;
    }
    report("hide", count, count);
}
act();
)";

/// Restores and activates the Thunderbird window.
const char *kRestoreScript = R"(
function act() {
    var clients = workspace.windowList();
    var count = 0;
    for (var i = 0; i < clients.length; ++i) {
        if (!isThunderbird(clients[i])) {
            continue;
        }
        count++;
        clients[i].minimized = false;
        workspace.activeWindow = clients[i];
    }
    report("restore", count, 0);
}
act();
)";

/// Asks whether the Thunderbird window is minimized.
const char *kStateScript = R"(
function act() {
    var clients = workspace.windowList();
    var count = 0;
    var hidden = 0;
    for (var i = 0; i < clients.length; ++i) {
        if (!isThunderbird(clients[i])) {
            continue;
        }
        count++;
        if (clients[i].minimized) {
            hidden++;
        }
    }
    report("state", count, hidden);
}
act();
)";
} // namespace

ThunderbirdWindow::ThunderbirdWindow(QObject *parent)
    : QObject(parent)
{
    // The script of the desktop calls back into this object.
    QDBusConnection::sessionBus().registerObject(QString::fromLatin1(kObjectPath),
                                                 this,
                                                 QDBusConnection::ExportAllSlots);
    QDBusConnection::sessionBus().registerService(QString::fromLatin1(kServiceName));
}

ThunderbirdWindow::~ThunderbirdWindow() = default;

bool ThunderbirdWindow::isSupported()
{
    QDBusInterface scripting(QStringLiteral("org.kde.KWin"),
                             QStringLiteral("/Scripting"),
                             QStringLiteral("org.kde.kwin.Scripting"),
                             QDBusConnection::sessionBus());
    return scripting.isValid();
}

bool ThunderbirdWindow::isHidden() const
{
    return m_hidden;
}

int ThunderbirdWindow::windowCount() const
{
    return m_windowCount;
}

void ThunderbirdWindow::hideWindow()
{
    runScript(QString::fromLatin1(kHideScript), QStringLiteral("hide"));
}

void ThunderbirdWindow::showWindow()
{
    runScript(QString::fromLatin1(kRestoreScript), QStringLiteral("restore"));
}

void ThunderbirdWindow::refreshState()
{
    runScript(QString::fromLatin1(kStateScript), QStringLiteral("state"));
}

void ThunderbirdWindow::snooze(int minutes)
{
    Q_EMIT snoozeRequested(minutes);
}

void ThunderbirdWindow::runScript(const QString &script, const QString & /*action*/)
{
    if (!isSupported()) {
        return;
    }

    QTemporaryFile file(QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                         + QStringLiteral("/thunderbird-tray-XXXXXX.js"));
    if (!file.open()) {
        return;
    }
    const QByteArray content = QByteArray(kScriptPrologue) + script.toUtf8();
    file.write(content);
    file.flush();

    QDBusInterface scripting(QStringLiteral("org.kde.KWin"),
                             QStringLiteral("/Scripting"),
                             QStringLiteral("org.kde.kwin.Scripting"),
                             QDBusConnection::sessionBus());

    // A script of the same name can only be loaded once.
    scripting.call(QStringLiteral("unloadScript"), QString::fromLatin1(kScriptName));

    const QDBusReply<int> id = scripting.call(QStringLiteral("loadScript"),
                                               file.fileName(),
                                               QString::fromLatin1(kScriptName));
    if (!id.isValid() || id.value() < 0) {
        return;
    }

    scripting.call(QStringLiteral("start"));
}

void ThunderbirdWindow::windowAction(const QString &action, int count, int hidden)
{
    m_windowCount = count;

    if (action == QLatin1String("hide")) {
        m_hidden = count > 0;
    } else if (action == QLatin1String("restore")) {
        m_hidden = false;
    } else if (action == QLatin1String("state")) {
        m_hidden = hidden > 0;
    }

    Q_EMIT stateChanged();
}
