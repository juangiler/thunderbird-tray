/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Juan Giler <contact@example.org>
 */

#include <QtTest>

#include "tbparser.h"

class TestTbParser : public QObject
{
    Q_OBJECT

private slots:
    void splitFolderPath_data();
    void splitFolderPath();
    void splitFolderPathRejectsGarbage_data();
    void splitFolderPathRejectsGarbage();

    void parsePrefs();
    void parseServers();
    void parseIdentities();
    void isVirtualFolderName_data();
    void isVirtualFolderName();
    void parseFolderCache();
};

namespace
{
/// A minimal but realistic prefs.js excerpt.
///
/// Note that the local folders account (server3) has no identity, exactly
/// like in a real Thunderbird profile.
const char *kPrefs = R"(
user_pref("mail.accountmanager.accounts", "account1,account2,account3");
user_pref("mail.account.account1.server", "server2");
user_pref("mail.account.account1.identities", "id1");
user_pref("mail.account.account2.server", "server3");
user_pref("mail.account.account3.server", "server4");
user_pref("mail.account.account3.identities", "id2");
user_pref("mail.identity.id1.useremail", "first@example.org");
user_pref("mail.identity.id2.useremail", "second@example.org");
user_pref("mail.server.server2.hostname", "imap.example.org");
user_pref("mail.server.server2.name", "First Account");
user_pref("mail.server.server2.type", "imap");
user_pref("mail.server.server2.directory", "/home/user/.thunderbird/default/ImapMail/imap.example.org");
user_pref("mail.server.server3.hostname", "Local Folders");
user_pref("mail.server.server3.name", "Local Folders");
user_pref("mail.server.server3.type", "none");
user_pref("mail.server.server3.directory", "/home/user/.thunderbird/default/Mail/Local Folders");
user_pref("mail.server.server4.hostname", "imap.other.org");
user_pref("mail.server.server4.name", "Second Account");
user_pref("mail.server.server4.type", "imap");
user_pref("mail.server.server4.directory", "/home/user/.thunderbird/default/ImapMail/imap.other.org");
)";

QHash<QString, QString> samplePrefs()
{
    return TbParser::parsePrefs(QString::fromLatin1(kPrefs));
}
} // namespace

void TestTbParser::splitFolderPath_data()
{
    QTest::addColumn<QString>("key");
    QTest::addColumn<QString>("accountId");
    QTest::addColumn<QString>("relativePath");

    // Regression test: the "/ImapMail/" marker is ten characters long. A
    // wrong length made every IMAP account resolve to "l".
    QTest::newRow("imap inbox")
        << "/home/user/.thunderbird/default/ImapMail/imap.gmail.com/INBOX.msf"
        << "imap.gmail.com" << "INBOX";
    QTest::newRow("imap sub folder")
        << "/home/user/.thunderbird/default/ImapMail/imap.gmail.com/[Gmail].sbd/Borradores.msf"
        << "imap.gmail.com" << "[Gmail]/Borradores";
    QTest::newRow("imap nested sub folder")
        << "/home/user/.thunderbird/default/ImapMail/imap.gmail.com/[Gmail].sbd/Sub.sbd/Deep.msf"
        << "imap.gmail.com" << "[Gmail]/Sub/Deep";
    QTest::newRow("local folders")
        << "/home/user/.thunderbird/default/Mail/Local Folders/Sent.msf"
        << "Local Folders" << "Sent";
    QTest::newRow("flatpak path")
        << "/home/user/.var/app/org.mozilla.thunderbird/.thunderbird/abc.default-release/ImapMail/imap.mail.me.com/INBOX.msf"
        << "imap.mail.me.com" << "INBOX";
}

void TestTbParser::splitFolderPath()
{
    QFETCH(QString, key);
    QFETCH(QString, accountId);
    QFETCH(QString, relativePath);

    const TbParser::FolderPath path = TbParser::splitFolderPath(key);

    QVERIFY2(path.valid, qPrintable(QStringLiteral("path was rejected: %1").arg(key)));
    QCOMPARE(path.accountId, accountId);
    QCOMPARE(path.relativePath, relativePath);
}

void TestTbParser::splitFolderPathRejectsGarbage_data()
{
    QTest::addColumn<QString>("key");

    QTest::newRow("empty") << QString();
    QTest::newRow("no marker") << "/home/user/.thunderbird/default/foo.msf";
    QTest::newRow("no extension") << "/home/user/.thunderbird/default/ImapMail/imap.gmail.com/INBOX";
    QTest::newRow("no account") << "/home/user/.thunderbird/default/ImapMail/INBOX.msf";
}

void TestTbParser::splitFolderPathRejectsGarbage()
{
    QFETCH(QString, key);

    const TbParser::FolderPath path = TbParser::splitFolderPath(key);

    QVERIFY(!path.valid);
}

void TestTbParser::parsePrefs()
{
    const QHash<QString, QString> prefs = samplePrefs();

    QCOMPARE(prefs.value(QStringLiteral("mail.accountmanager.accounts")),
             QStringLiteral("account1,account2,account3"));
    QCOMPARE(prefs.value(QStringLiteral("mail.identity.id1.useremail")),
             QStringLiteral("first@example.org"));
    QCOMPARE(prefs.value(QStringLiteral("mail.server.server2.hostname")),
             QStringLiteral("imap.example.org"));
}

void TestTbParser::parseServers()
{
    const QHash<QString, TbParser::ServerInfo> servers = TbParser::parseServers(samplePrefs());

    QCOMPARE(servers.size(), 3);

    const TbParser::ServerInfo imap = servers.value(QStringLiteral("server2"));
    QCOMPARE(imap.key, QStringLiteral("server2"));
    QCOMPARE(imap.hostname, QStringLiteral("imap.example.org"));
    QCOMPARE(imap.name, QStringLiteral("First Account"));
    QCOMPARE(imap.type, QStringLiteral("imap"));
    QCOMPARE(imap.directory,
             QStringLiteral("/home/user/.thunderbird/default/ImapMail/imap.example.org"));

    const TbParser::ServerInfo local = servers.value(QStringLiteral("server3"));
    QCOMPARE(local.hostname, QStringLiteral("Local Folders"));
    QCOMPARE(local.type, QStringLiteral("none"));
}

void TestTbParser::parseIdentities()
{
    const QHash<QString, QString> prefs = samplePrefs();
    const QHash<QString, QString> identities = TbParser::parseIdentities(prefs);

    QCOMPARE(identities.value(QStringLiteral("server2")), QStringLiteral("first@example.org"));
    QCOMPARE(identities.value(QStringLiteral("server4")), QStringLiteral("second@example.org"));
    // The local folders have no identity.
    QVERIFY(!identities.contains(QStringLiteral("server3")));
    QCOMPARE(identities.size(), 2);
}

void TestTbParser::isVirtualFolderName_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<bool>("expected");

    QTest::newRow("todos") << "Todos" << true;
    QTest::newRow("all mail") << "All Mail" << true;
    QTest::newRow("unified") << "Unified" << true;
    QTest::newRow("inbox") << "INBOX" << false;
    QTest::newRow("sent") << "Sent" << false;
}

void TestTbParser::isVirtualFolderName()
{
    QFETCH(QString, name);
    QFETCH(bool, expected);

    QCOMPARE(TbParser::isVirtualFolderName(name), expected);
}

void TestTbParser::parseFolderCache()
{
    const QByteArray json = R"({
        "/home/user/.thunderbird/default/ImapMail/imap.example.org/INBOX.msf": {
            "totalUnreadMsgs": 7, "totalMsgs": 42
        },
        "/home/user/.thunderbird/default/ImapMail/imap.example.org/[Gmail].sbd/Todos.msf": {
            "totalUnreadMsgs": 3, "totalMsgs": 9
        },
        "/home/user/.thunderbird/default/Mail/Local Folders/Sent.msf": {
            "totalUnreadMsgs": 0, "totalMsgs": 5
        }
    })";

    const QVector<TbFolder> folders =
        TbParser::parseFolderCache(json, samplePrefs(), QStringLiteral("/home/user/.thunderbird/default"));

    QCOMPARE(folders.size(), 3);

    const auto find = [&folders](const QString &id) -> TbFolder {
        for (const TbFolder &folder : folders) {
            if (folder.id == id) {
                return folder;
            }
        }
        return TbFolder();
    };

    // The IMAP inbox.
    const TbFolder inbox = find(QStringLiteral("imap.example.org/INBOX"));
    QCOMPARE(inbox.accountId, QStringLiteral("imap.example.org"));
    QCOMPARE(inbox.accountName, QStringLiteral("first@example.org"));
    QCOMPARE(inbox.folderName, QStringLiteral("INBOX"));
    QCOMPARE(inbox.relativePath, QStringLiteral("INBOX"));
    QCOMPARE(inbox.unread, 7);
    QCOMPARE(inbox.total, 42);
    QVERIFY(inbox.isInbox);
    QVERIFY(!inbox.isVirtual);
    QCOMPARE(inbox.absolutePath,
             QStringLiteral("/home/user/.thunderbird/default/ImapMail/imap.example.org/INBOX.msf"));

    // A virtual folder of the same account.
    const TbFolder todos = find(QStringLiteral("imap.example.org/[Gmail]/Todos"));
    QCOMPARE(todos.accountId, QStringLiteral("imap.example.org"));
    QCOMPARE(todos.accountName, QStringLiteral("first@example.org"));
    QCOMPARE(todos.folderName, QStringLiteral("Todos"));
    QCOMPARE(todos.relativePath, QStringLiteral("[Gmail]/Todos"));
    QVERIFY(todos.isVirtual);

    // The local folders have no identity, the server name is used instead.
    const TbFolder sent = find(QStringLiteral("Local Folders/Sent"));
    QCOMPARE(sent.accountId, QStringLiteral("Local Folders"));
    QCOMPARE(sent.accountName, QStringLiteral("Local Folders"));
    QCOMPARE(sent.unread, 0);
    QCOMPARE(sent.total, 5);
    QVERIFY(!sent.isInbox);
}

QTEST_GUILESS_MAIN(TestTbParser)

#include "tst_tbparser.moc"
