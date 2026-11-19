/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Juan Giler <contact@example.org>
 */

#include "foldermodel.h"

FolderListModel::FolderListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int FolderListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_rows.size());
}

QVariant FolderListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }

    const Row &row = m_rows.at(index.row());
    const TbFolder &folder = row.folder;

    switch (role) {
    case Qt::DisplayRole:
        return folder.folderName;
    case AccountRole:
        return folder.accountName;
    case FolderRole:
        return folder.folderName;
    case UnreadRole:
        return folder.unread;
    case TotalRole:
        return folder.total;
    case FolderIdRole:
        return folder.id;
    case MonitoredRole:
        return row.enabled;
    case IsInboxRole:
        return folder.isInbox;
    case IsVirtualRole:
        return folder.isVirtual;
    default:
        return {};
    }
}

QHash<int, QByteArray> FolderListModel::roleNames() const
{
    return {
        {Qt::DisplayRole, QByteArrayLiteral("display")},
        {AccountRole, QByteArrayLiteral("account")},
        {FolderRole, QByteArrayLiteral("folder")},
        {UnreadRole, QByteArrayLiteral("unread")},
        {TotalRole, QByteArrayLiteral("total")},
        {FolderIdRole, QByteArrayLiteral("folderId")},
        {MonitoredRole, QByteArrayLiteral("monitored")},
        {IsInboxRole, QByteArrayLiteral("inbox")},
        {IsVirtualRole, QByteArrayLiteral("virtual")},
    };
}

void FolderListModel::setFolders(const QVector<TbFolder> &folders, const QSet<QString> &monitoredIds)
{
    // Did the list of folders change? Then the model has to be rebuilt.
    bool sameLayout = folders.size() == m_rows.size();
    if (sameLayout) {
        for (int i = 0; i < folders.size(); ++i) {
            if (folders.at(i).id != m_rows.at(i).folder.id) {
                sameLayout = false;
                break;
            }
        }
    }

    if (!sameLayout) {
        beginResetModel();

        m_rows.clear();
        m_accounts.clear();

        for (const TbFolder &folder : folders) {
            Row row;
            row.folder = folder;
            row.enabled = monitoredIds.contains(folder.id);
            m_rows.append(row);
            m_accounts.insert(folder.accountId);
        }

        endResetModel();
        Q_EMIT countChanged();
        return;
    }

    // Same folders: update the rows that changed and leave the rest alone.
    for (int i = 0; i < folders.size(); ++i) {
        const TbFolder &incoming = folders.at(i);
        Row &row = m_rows[i];

        const bool monitored = monitoredIds.contains(incoming.id);
        const bool changed = row.folder.unread != incoming.unread
            || row.folder.total != incoming.total
            || row.folder.accountName != incoming.accountName
            || row.folder.folderName != incoming.folderName
            || row.folder.isInbox != incoming.isInbox
            || row.folder.isVirtual != incoming.isVirtual
            || row.enabled != monitored;

        if (!changed) {
            continue;
        }

        row.folder = incoming;
        row.enabled = monitored;

        const QModelIndex idx = index(i, 0);
        Q_EMIT dataChanged(idx, idx);
    }

    Q_EMIT countChanged();
}

bool FolderListModel::setFolderEnabled(const QString &folderId, bool enabled)
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).folder.id != folderId) {
            continue;
        }
        if (m_rows.at(i).enabled == enabled) {
            return true;
        }
        m_rows[i].enabled = enabled;
        const QModelIndex idx = index(i, 0);
        Q_EMIT dataChanged(idx, idx, {MonitoredRole});
        Q_EMIT countChanged();
        return true;
    }

    // Unknown folder: nothing to change, and nothing to remember either.
    return false;
}

bool FolderListModel::isFolderMonitored(const QString &folderId) const
{
    for (const Row &row : m_rows) {
        if (row.folder.id == folderId) {
            return row.enabled;
        }
    }
    return false;
}

QSet<QString> FolderListModel::monitoredFolderIds() const
{
    QSet<QString> ids;
    for (const Row &row : m_rows) {
        if (row.enabled) {
            ids.insert(row.folder.id);
        }
    }
    return ids;
}

FolderListModel::Totals FolderListModel::totals() const
{
    Totals totals;
    for (const Row &row : m_rows) {
        if (!row.enabled) {
            continue;
        }
        totals.unread += row.folder.unread;
        totals.total += row.folder.total;
    }
    return totals;
}

int FolderListModel::folderCount() const
{
    return static_cast<int>(m_rows.size());
}

int FolderListModel::accountCount() const
{
    return static_cast<int>(m_accounts.size());
}

int FolderListModel::monitoredCount() const
{
    int count = 0;
    for (const Row &row : m_rows) {
        if (row.enabled) {
            ++count;
        }
    }
    return count;
}
