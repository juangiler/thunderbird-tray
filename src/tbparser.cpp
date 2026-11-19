/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Juan Giler <contact@example.org>
 */

#include "tbparser.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QTextStream>

namespace
{
/// Directory that holds the folders of an IMAP account.
const QString kImapStore = QStringLiteral("/ImapMail/");
/// Directory that holds the local folders.
const QString kMailStore = QStringLiteral("/Mail/");
} // namespace

namespace TbParser
{

FolderPath splitFolderPath(const QString &key)
{
    FolderPath result;

    const QString normalised = QDir::fromNativeSeparators(key);

    // Thunderbird stores IMAP accounts in "ImapMail" and local folders in
    // "Mail". Both markers are searched for, the IMAP one first.
    int index = normalised.lastIndexOf(kImapStore);
    QString storePrefix = kImapStore.mid(1);
    if (index < 0) {
        index = normalised.lastIndexOf(kMailStore);
        storePrefix = kMailStore.mid(1);
    }
    if (index < 0) {
        return result;
    }

    // "ImapMail/imap.gmail.com/INBOX.msf"
    QString storePath = normalised.mid(index + 1);
    if (!storePath.endsWith(QLatin1String(".msf"), Qt::CaseInsensitive)) {
        return result;
    }

    QString withoutExtension = storePath;
    withoutExtension.chop(4);

    // Skip the store directory itself: "imap.gmail.com/INBOX"
    const QString accountAndPath = withoutExtension.mid(storePrefix.size());
    const int slash = accountAndPath.indexOf(QLatin1Char('/'));
    if (slash <= 0) {
        return result;
    }

    result.accountId = accountAndPath.left(slash);

    // Sub folders live in a ".sbd" directory, e.g. "[Gmail].sbd/Todos.msf".
    QString relativePath = accountAndPath.mid(slash + 1);
    relativePath.replace(QLatin1String(".sbd/"), QLatin1String("/"));

    result.relativePath = relativePath;
    result.storePath = storePath;
    result.valid = true;
    return result;
}

QHash<QString, QString> parsePrefs(const QString &text)
{
    QHash<QString, QString> prefs;

    static const QRegularExpression expression(
        QStringLiteral("user_pref\\s*\\(\\s*\"([^\"]+)\"\\s*,\\s*(.+?)\\s*\\)\\s*;"));

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const QRegularExpressionMatch match = expression.match(line);
        if (!match.hasMatch()) {
            continue;
        }
        QString value = match.captured(2).trimmed();
        if (value.size() >= 2 && value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"'))) {
            value = value.mid(1, value.size() - 2);
        }
        prefs.insert(match.captured(1), value);
    }

    return prefs;
}

QHash<QString, QString> readPrefsFile(const QString &path)
{
    QHash<QString, QString> prefs;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return prefs;
    }

    return parsePrefs(QString::fromUtf8(file.readAll()));
}

QHash<QString, ServerInfo> parseServers(const QHash<QString, QString> &prefs)
{
    QHash<QString, ServerInfo> servers;

    static const QRegularExpression expression(QStringLiteral("^mail\\.server\\.(server\\d+)\\.(.+)$"));

    for (auto it = prefs.constBegin(); it != prefs.constEnd(); ++it) {
        const QRegularExpressionMatch match = expression.match(it.key());
        if (!match.hasMatch()) {
            continue;
        }
        ServerInfo &server = servers[match.captured(1)];
        server.key = match.captured(1);
        const QString &field = match.captured(2);
        if (field == QLatin1String("hostname")) {
            server.hostname = it.value();
        } else if (field == QLatin1String("name")) {
            server.name = it.value();
        } else if (field == QLatin1String("type")) {
            server.type = it.value();
        } else if (field == QLatin1String("directory")) {
            server.directory = it.value();
        }
    }

    return servers;
}

QHash<QString, QString> parseIdentities(const QHash<QString, QString> &prefs)
{
    QHash<QString, QString> result;

    const QString accounts = prefs.value(QStringLiteral("mail.accountmanager.accounts"));
    if (accounts.isEmpty()) {
        return result;
    }

    const QStringList accountIds = accounts.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &accountId : accountIds) {
        const QString accountKey = accountId.trimmed();
        const QString serverKey = prefs.value(QStringLiteral("mail.account.%1.server").arg(accountKey));
        if (serverKey.isEmpty()) {
            continue;
        }
        const QString identityList = prefs.value(QStringLiteral("mail.account.%1.identities").arg(accountKey));
        const QStringList identityIds = identityList.split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const QString &identityId : identityIds) {
            const QString email =
                prefs.value(QStringLiteral("mail.identity.%1.useremail").arg(identityId.trimmed()));
            if (!email.isEmpty()) {
                result.insert(serverKey, email);
                break;
            }
        }
    }

    return result;
}

bool isVirtualFolderName(const QString &name)
{
    static const QStringList virtualNames = {
        QStringLiteral("todos"),
        QStringLiteral("all mail"),
        QStringLiteral("unified"),
        QStringLiteral("smart mailboxes"),
    };
    return virtualNames.contains(name.toLower());
}

QString accountDisplayName(const ServerInfo &server, const QHash<QString, QString> &identities)
{
    const QString email = identities.value(server.key);
    if (!email.isEmpty()) {
        return email;
    }
    if (!server.hostname.isEmpty()) {
        return server.hostname;
    }
    return server.name;
}

QVector<TbFolder> parseFolderCache(const QByteArray &json,
                                   const QHash<QString, QString> &prefs,
                                   const QString &profilePath,
                                   QStringList *warnings)
{
    QVector<TbFolder> folders;

    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        if (warnings) {
            *warnings << QStringLiteral("folderCache.json could not be parsed, it is probably being written right now.");
        }
        return folders;
    }

    const QHash<QString, ServerInfo> servers = parseServers(prefs);
    const QHash<QString, QString> identities = parseIdentities(prefs);

    // Map the name of an on-disk account directory to its server entry.
    QHash<QString, const ServerInfo *> serverByDirectory;
    for (auto it = servers.constBegin(); it != servers.constEnd(); ++it) {
        const ServerInfo &server = it.value();
        if (!server.hostname.isEmpty()) {
            serverByDirectory.insert(server.hostname, &server);
        }
        if (!server.directory.isEmpty()) {
            const QString base = QFileInfo(server.directory).fileName();
            if (!base.isEmpty()) {
                serverByDirectory.insert(base, &server);
            }
        }
    }

    const QJsonObject cache = document.object();
    for (auto it = cache.constBegin(); it != cache.constEnd(); ++it) {
        const QString key = it.key();
        if (!key.endsWith(QLatin1String(".msf"), Qt::CaseInsensitive)) {
            continue;
        }

        const FolderPath path = splitFolderPath(key);
        if (!path.valid) {
            continue;
        }

        TbFolder folder;
        folder.accountId = path.accountId;
        folder.relativePath = path.relativePath;
        folder.folderName = path.relativePath.section(QLatin1Char('/'), -1);
        folder.id = path.accountId + QLatin1Char('/') + path.relativePath;
        folder.absolutePath = profilePath + QLatin1Char('/') + path.storePath;
        folder.isInbox = folder.folderName.compare(QLatin1String("INBOX"), Qt::CaseInsensitive) == 0;
        folder.isVirtual = isVirtualFolderName(folder.folderName);

        const QJsonObject summary = it.value().toObject();
        const int unread = summary.value(QStringLiteral("totalUnreadMsgs")).toInt(-1);
        const int total = summary.value(QStringLiteral("totalMsgs")).toInt(-1);
        folder.unread = unread > 0 ? unread : 0;
        folder.total = total > 0 ? total : 0;

        const ServerInfo *server = serverByDirectory.value(path.accountId, nullptr);
        folder.accountName = server ? accountDisplayName(*server, identities) : path.accountId;

        folders.append(folder);
    }

    return folders;
}

} // namespace TbParser
