/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Juan Giler <contact@example.org>
 */

#include <QtTest>

#include "foldermodel.h"

class TestFolderListModel : public QObject
{
    Q_OBJECT

private slots:
    void roleNamesDoNotShadowItemProperties();
    void setFoldersPopulatesRows();
    void setFolderEnabledTogglesTheRole();
    void setFolderEnabledRejectsUnknownIds();
    void totalsOnlyCountMonitoredFolders();
    void setFoldersKeepsTheModelWhenNothingChanged();
    void setFoldersUpdatesRowsInPlace();
    void setFoldersResetsWhenFoldersDisappear();
};

namespace
{
TbFolder makeFolder(const QString &id, int unread = 0, int total = 0)
{
    TbFolder folder;
    folder.id = id;
    folder.accountId = id.section(QLatin1Char('/'), 0, 0);
    folder.accountName = folder.accountId + QStringLiteral("@example.org");
    folder.folderName = id.section(QLatin1Char('/'), -1);
    folder.relativePath = folder.folderName;
    folder.unread = unread;
    folder.total = total;
    folder.isInbox = folder.folderName.compare(QLatin1String("INBOX"), Qt::CaseInsensitive) == 0;
    return folder;
}
} // namespace

void TestFolderListModel::roleNamesDoNotShadowItemProperties()
{
    // A role called "enabled" would shadow Item.enabled in a QML delegate
    // and disable the row itself.
    const FolderListModel model;
    QVERIFY(!model.roleNames().values().contains(QByteArrayLiteral("enabled")));
    QVERIFY(model.roleNames().values().contains(QByteArrayLiteral("monitored")));
}

void TestFolderListModel::setFoldersPopulatesRows()
{
    FolderListModel model;
    model.setFolders({makeFolder(QStringLiteral("imap.example.org/INBOX"), 7, 42)}, {});

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.accountCount(), 1);

    const QModelIndex index = model.index(0, 0);
    QCOMPARE(index.data(FolderListModel::FolderRole).toString(), QStringLiteral("INBOX"));
    QCOMPARE(index.data(FolderListModel::AccountRole).toString(),
             QStringLiteral("imap.example.org@example.org"));
    QCOMPARE(index.data(FolderListModel::UnreadRole).toInt(), 7);
    QCOMPARE(index.data(FolderListModel::TotalRole).toInt(), 42);
    QCOMPARE(index.data(FolderListModel::FolderIdRole).toString(),
             QStringLiteral("imap.example.org/INBOX"));
    QVERIFY(index.data(FolderListModel::IsInboxRole).toBool());
}

void TestFolderListModel::setFolderEnabledTogglesTheRole()
{
    FolderListModel model;
    model.setFolders({makeFolder(QStringLiteral("imap.example.org/INBOX"), 7, 42)}, {});

    QVERIFY(model.setFolderEnabled(QStringLiteral("imap.example.org/INBOX"), true));
    QVERIFY(model.isFolderMonitored(QStringLiteral("imap.example.org/INBOX")));
    QCOMPARE(model.index(0, 0).data(FolderListModel::MonitoredRole).toBool(), true);

    QVERIFY(model.setFolderEnabled(QStringLiteral("imap.example.org/INBOX"), false));
    QVERIFY(!model.isFolderMonitored(QStringLiteral("imap.example.org/INBOX")));
    QCOMPARE(model.index(0, 0).data(FolderListModel::MonitoredRole).toBool(), false);
}

void TestFolderListModel::setFolderEnabledRejectsUnknownIds()
{
    FolderListModel model;
    model.setFolders({makeFolder(QStringLiteral("imap.example.org/INBOX"), 7, 42)}, {});

    // An unknown id must not end up in the persisted list of monitored
    // folders.
    QVERIFY(!model.setFolderEnabled(QStringLiteral("does/not/exist"), true));
    QVERIFY(model.monitoredFolderIds().isEmpty());
}

void TestFolderListModel::totalsOnlyCountMonitoredFolders()
{
    FolderListModel model;
    model.setFolders({makeFolder(QStringLiteral("a/INBOX"), 7, 42),
                      makeFolder(QStringLiteral("b/INBOX"), 3, 10),
                      makeFolder(QStringLiteral("c/Sent"), 5, 5)},
                     {QStringLiteral("a/INBOX"), QStringLiteral("b/INBOX")});

    const FolderListModel::Totals totals = model.totals();
    QCOMPARE(totals.unread, 10);
    QCOMPARE(totals.total, 52);
    QCOMPARE(model.monitoredCount(), 2);

    model.setFolderEnabled(QStringLiteral("b/INBOX"), false);
    const FolderListModel::Totals after = model.totals();
    QCOMPARE(after.unread, 7);
    QCOMPARE(after.total, 42);
}

void TestFolderListModel::setFoldersKeepsTheModelWhenNothingChanged()
{
    FolderListModel model;
    const QVector<TbFolder> folders = {makeFolder(QStringLiteral("a/INBOX"), 7, 42)};
    model.setFolders(folders, {QStringLiteral("a/INBOX")});

    // A poll that finds the same state must not reset the model: that
    // would recreate every delegate in the QML view.
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    QSignalSpy changedSpy(&model, &QAbstractItemModel::dataChanged);

    model.setFolders(folders, {QStringLiteral("a/INBOX")});

    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(changedSpy.count(), 0);
}

void TestFolderListModel::setFoldersUpdatesRowsInPlace()
{
    FolderListModel model;
    model.setFolders({makeFolder(QStringLiteral("a/INBOX"), 7, 42)}, {QStringLiteral("a/INBOX")});

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    QSignalSpy changedSpy(&model, &QAbstractItemModel::dataChanged);

    model.setFolders({makeFolder(QStringLiteral("a/INBOX"), 9, 44)}, {QStringLiteral("a/INBOX")});

    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(model.index(0, 0).data(FolderListModel::UnreadRole).toInt(), 9);
}

void TestFolderListModel::setFoldersResetsWhenFoldersDisappear()
{
    FolderListModel model;
    model.setFolders({makeFolder(QStringLiteral("a/INBOX"), 7, 42),
                      makeFolder(QStringLiteral("b/INBOX"), 3, 10)},
                     {QStringLiteral("a/INBOX")});

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    model.setFolders({makeFolder(QStringLiteral("a/INBOX"), 7, 42)}, {QStringLiteral("a/INBOX")});

    QCOMPARE(resetSpy.count(), 1);
    QCOMPARE(model.rowCount(), 1);
}

QTEST_GUILESS_MAIN(TestFolderListModel)

#include "tst_foldermodel.moc"
