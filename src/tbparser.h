/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Juan Giler <contact@example.org>
 */

#pragma once

#include <QHash>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

/**
 * A single mail folder discovered inside a Thunderbird profile.
 *
 * The data is derived from Thunderbird's `folderCache.json`, which is the
 * authoritative per-folder summary written by Thunderbird 115 and newer.
 */
struct TbFolder
{
    QString id;           ///< "<accountId>/<relative path>", stable across restarts
    QString accountId;    ///< e.g. "imap.gmail.com"
    QString accountName;  ///< human readable, e.g. "juanjosegilerm@gmail.com"
    QString folderName;   ///< e.g. "INBOX"
    QString relativePath; ///< e.g. "INBOX" or "[Gmail]/Todos"
    QString absolutePath; ///< absolute path of the backing .msf file
    int unread = 0;       ///< unread messages known to Thunderbird
    int total = 0;        ///< total messages known to Thunderbird
    bool isInbox = false; ///< true for INBOX style folders
    bool isVirtual = false; ///< true for aggregate folders ("All Mail", "Unified")
};
Q_DECLARE_METATYPE(TbFolder)

/**
 * Pure, side effect free parsing of the files Thunderbird leaves behind.
 *
 * Everything in here works on plain strings and hashes so that it can be
 * unit tested without touching the file system or a real Thunderbird
 * installation.
 */
namespace TbParser
{
/// A "server" entry of Thunderbird's prefs.js, one per mail account.
struct ServerInfo
{
    QString key;       ///< server key, e.g. "server2"
    QString hostname;  ///< e.g. "imap.gmail.com"
    QString name;      ///< display name of the account
    QString type;      ///< "imap", "pop3", "none" (for local folders)
    QString directory; ///< directory that holds the account's folders
};

/// The result of splitting a folderCache.json key.
struct FolderPath
{
    bool valid = false;
    QString accountId;    ///< e.g. "imap.gmail.com"
    QString relativePath; ///< e.g. "INBOX" or "[Gmail]/Todos"
    QString storePath;    ///< profile relative path of the .msf file
};

/**
 * Splits a folderCache.json key into its account and folder parts.
 *
 * The keys look like
 * "/home/user/.thunderbird/<profile>/ImapMail/imap.gmail.com/INBOX.msf"
 * or
 * "/home/user/.thunderbird/<profile>/Mail/Local Folders/Sent.msf".
 *
 * Sub folders are stored in a ".sbd" directory; the ".sbd" is removed from
 * the relative path but kept in the store path.
 */
FolderPath splitFolderPath(const QString &key);

/// Parses the content of a prefs.js file into a flat key/value map.
QHash<QString, QString> parsePrefs(const QString &text);

/// Reads and parses a prefs.js file. Returns an empty map when unreadable.
QHash<QString, QString> readPrefsFile(const QString &path);

/// Collects every "mail.server.*" entry of a prefs map.
QHash<QString, ServerInfo> parseServers(const QHash<QString, QString> &prefs);

/// Maps a server key to the e-mail address of the account's first identity.
QHash<QString, QString> parseIdentities(const QHash<QString, QString> &prefs);

/// True for aggregate folders that mirror the content of other folders.
bool isVirtualFolderName(const QString &name);

/// Human readable name of the account a server belongs to.
QString accountDisplayName(const ServerInfo &server, const QHash<QString, QString> &identities);

/**
 * Parses a folderCache.json document into a list of folders.
 *
 * @param json        the raw content of folderCache.json
 * @param prefs       the parsed prefs.js of the same profile
 * @param profilePath the real path of the profile on this machine
 * @param warnings    optional list that receives human readable problems
 */
QVector<TbFolder> parseFolderCache(const QByteArray &json,
                                   const QHash<QString, QString> &prefs,
                                   const QString &profilePath,
                                   QStringList *warnings = nullptr);

} // namespace TbParser
