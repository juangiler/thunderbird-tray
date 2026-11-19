/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Juan Giler <contact@example.org>
 */

#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTimer>

#include "backend.h"
#include "foldermodel.h"

#include <QTextStream>

#include <functional>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace
{
/**
 * Name of the local socket that is used to detect a second instance.
 *
 * The user id is part of the name so that instances of different users on
 * the same machine do not collide in the world writable temporary
 * directory.
 */
QString singleInstanceSocketName()
{
#ifdef Q_OS_UNIX
    return QStringLiteral("thunderbird-tray-%1").arg(getuid());
#else
    return QStringLiteral("thunderbird-tray");
#endif
}

/**
 * Ensures that only one instance of the application is running.
 *
 * A second instance forwards a "show" request to the running one and exits.
 */
class SingleInstanceGuard : public QObject
{
public:
    /// Called when another instance asked this one to show its window.
    using ShowCallback = std::function<void()>;

    explicit SingleInstanceGuard(QObject *parent)
        : QObject(parent)
        , m_server(new QLocalServer(this))
    {
        connect(m_server, &QLocalServer::newConnection, this, [this]() {
            QLocalSocket *socket = m_server->nextPendingConnection();
            if (!socket) {
                return;
            }
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
                if (socket->readAll().contains("show") && m_showCallback) {
                    m_showCallback();
                }
            });
        });
    }

    void setShowCallback(ShowCallback callback)
    {
        m_showCallback = std::move(callback);
    }

    /**
     * Tries to become the only instance.
     *
     * @return false when another instance is already running and was asked
     *         to show itself; in that case this process has to exit.
     */
    bool tryAcquire()
    {
        const QString name = singleInstanceSocketName();

        // Listen first: if the address is free, this is the first instance.
        //
        // The socket options must stay at their default here. With
        // UserAccessOption Qt binds a temporary socket and renames it over
        // the target, which silently replaces the socket of a running
        // instance instead of failing with AddressInUseError - two
        // instances would then run side by side.
        if (m_server->listen(name)) {
            restrictSocketToUser();
            return true;
        }

        if (m_server->serverError() != QAbstractSocket::AddressInUseError) {
            // Something else is wrong; the application is still usable.
            return true;
        }

        // Somebody owns the socket: forward a "show" request.
        QLocalSocket socket;
        socket.connectToServer(name);
        if (socket.waitForConnected(300)) {
            socket.write("show");
            socket.flush();
            socket.waitForBytesWritten(300);
            return false;
        }

        // The socket is stale, the owner crashed. Remove it and retry.
        QLocalServer::removeServer(name);
        if (!m_server->listen(name)) {
            return true; // The guard is best effort.
        }
        restrictSocketToUser();
        return true;
    }

private:
    /// Only the user that started the application may talk to it.
    void restrictSocketToUser() const
    {
        const QString path = m_server->fullServerName();
        if (path.isEmpty()) {
            return;
        }
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }

    QLocalServer *m_server = nullptr;
    ShowCallback m_showCallback;
};
} // namespace

int main(int argc, char *argv[])
{
    // QApplication (and not QGuiApplication) is required because the tray
    // icon context menu is a QMenu, which lives in Qt Widgets.
    QApplication app(argc, argv);

    app.setOrganizationName(QStringLiteral("io.github.juangiler"));
    app.setOrganizationDomain(QStringLiteral("io.github.juangiler"));
    app.setApplicationName(QStringLiteral("thunderbird-tray"));
    app.setApplicationDisplayName(QStringLiteral("Thunderbird Tray"));
    app.setApplicationVersion(QStringLiteral("0.0.1"));
    app.setDesktopFileName(QStringLiteral("io.github.juangiler.thunderbird-tray"));
    app.setQuitOnLastWindowClosed(false);
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("mail-unread"), QIcon(QStringLiteral(":/resources/icons/thunderbird-tray.svg"))));

    // Inherit the native Breeze look and feel of the desktop.
    QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("System tray companion for Thunderbird"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption settingsOption({QStringLiteral("s"), QStringLiteral("settings")},
                                           QStringLiteral("Open the settings window on start"));
    parser.addOption(settingsOption);
    const QCommandLineOption dumpOption(QStringLiteral("dump-state"),
                                         QStringLiteral("Print the detected profile state and exit"));
    parser.addOption(dumpOption);
    parser.process(app);

    SingleInstanceGuard guard(&app);
    if (!guard.tryAcquire()) {
        // Another instance is already running and was asked to show itself.
        return 0;
    }

    AppBackend backend;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("AppBackend"), &backend);

    const auto showWindow = [&engine]() {
        const QList<QObject *> roots = engine.rootObjects();
        if (roots.isEmpty()) {
            return;
        }
        QMetaObject::invokeMethod(roots.constFirst(), "show");
    };

    // A second instance asks the running one to show its window.
    guard.setShowCallback(showWindow);

    QObject::connect(&backend, &AppBackend::showWindowRequested, &engine, showWindow);

    QObject::connect(&backend, &AppBackend::quitRequested, &app, &QCoreApplication::quit);

    // Show a window when the user asked for it on the command line.
    if (parser.isSet(settingsOption)) {
        QTimer::singleShot(0, &app, showWindow);
    }

    engine.loadFromModule("io.github.juangiler.thunderbirdtray", "Main");

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    if (parser.isSet(dumpOption)) {
        // Wait for the first scan to finish, then print what was detected.
        QTimer::singleShot(2500, &app, [&backend, &app]() {
            QTextStream out(stdout);
            out << "profile found :" << backend.profileFound() << Qt::endl;
            out << "profile path  :" << backend.profilePath() << Qt::endl;
            out << "profile name  :" << backend.profileName() << Qt::endl;
            out << "unread total  :" << backend.unreadCount() << Qt::endl;
            out << "folders       :" << backend.folders()->rowCount() << Qt::endl;
            out << "accent color  :" << backend.themeAccentColor() << Qt::endl;
            out << "badge color   :" << backend.currentBadgeColor() << Qt::endl;
            out << "tb windows    :" << backend.thunderbirdWindowCount() << Qt::endl;
            out << "tb hidden     :" << (backend.windowHidden() ? "yes" : "no") << Qt::endl;
            out << "badge account :" << (backend.badgeAccount().isEmpty()
                                            ? QStringLiteral("-")
                                            : backend.badgeAccount()) << Qt::endl;
            out << "dark theme    :" << (backend.darkTheme() ? "yes" : "no") << Qt::endl;
            for (const QVariant &entry : backend.accounts()) {
                const QVariantMap account = entry.toMap();
                out << "  account: " << account.value(QStringLiteral("name")).toString()
                    << " unread=" << account.value(QStringLiteral("unread")).toInt()
                    << " color=" << account.value(QStringLiteral("color")).toString() << Qt::endl;
            }
            const auto *model = qobject_cast<QAbstractListModel *>(backend.folders());
            if (!model) {
                QTextStream(stderr) << "warning: the folder model is not available" << Qt::endl;
                app.quit();
                return;
            }
            for (int row = 0; row < model->rowCount(); ++row) {
                const QModelIndex idx = model->index(row, 0);
                out << "  - " << idx.data(FolderListModel::FolderRole).toString() << " | "
                    << idx.data(FolderListModel::AccountRole).toString() << " | unread: "
                    << idx.data(FolderListModel::UnreadRole).toInt() << " | monitored: "
                    << (idx.data(FolderListModel::MonitoredRole).toBool() ? "yes" : "no") << Qt::endl;
            }
            const QStringList warnings = backend.warnings();
            for (const QString &warning : warnings) {
                QTextStream(stderr) << "warning: " << warning << Qt::endl;
            }
            out.flush();
            app.quit();
        });
    }

    if (!TrayIcon::isSystemTrayAvailable()) {
        showWindow();
    }

    return app.exec();
}
