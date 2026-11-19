/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Juan Giler <contact@example.org>
 */

#pragma once

#include <QAbstractListModel>
#include <QSet>
#include <QString>
#include <QVector>

#include "tbwatcher.h"

/**
 * Flat list model exposing the detected mail folders to QML.
 *
 * Every row is one folder together with the account it belongs to, so the
 * settings page can render a simple checkbox list without needing a nested
 * (tree) model.
 *
 * The role names are deliberately free of names that QML items already use
 * ("enabled", "visible", ...) so that a delegate can declare them as
 * required properties without shadowing an inherited property.
 */
class FolderListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int folderCount READ folderCount NOTIFY countChanged)
    Q_PROPERTY(int accountCount READ accountCount NOTIFY countChanged)
    Q_PROPERTY(int monitoredCount READ monitoredCount NOTIFY countChanged)

public:
    enum Roles {
        AccountRole = Qt::UserRole + 1,
        FolderRole,
        UnreadRole,
        TotalRole,
        FolderIdRole,
        MonitoredRole,
        IsInboxRole,
        IsVirtualRole,
    };
    Q_ENUM(Roles)

    /// The summed counters of all monitored folders.
    struct Totals
    {
        int unread = 0;
        int total = 0;
    };

    explicit FolderListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /**
     * Replaces the content of the model.
     *
     * When the set of folders did not change, the existing rows are updated
     * in place instead of resetting the model: a reset would recreate every
     * delegate in the QML view and make the list flicker on every poll.
     *
     * @param folders    the folders that were detected
     * @param monitoredIds ids of the folders that should be monitored
     */
    void setFolders(const QVector<TbFolder> &folders, const QSet<QString> &monitoredIds);

    /**
     * Marks a folder as monitored (or not).
     *
     * @return false when no folder with that id exists.
     */
    bool setFolderEnabled(const QString &folderId, bool enabled);

    /// Returns whether the given folder is monitored.
    bool isFolderMonitored(const QString &folderId) const;

    /// Ids of all monitored folders.
    QSet<QString> monitoredFolderIds() const;

    /// The summed counters of all monitored folders.
    Totals totals() const;

    int folderCount() const;
    int accountCount() const;
    int monitoredCount() const;

Q_SIGNALS:
    void countChanged();

private:
    struct Row
    {
        TbFolder folder;
        bool enabled = false;
    };

    QVector<Row> m_rows;
    QSet<QString> m_accounts;
};
