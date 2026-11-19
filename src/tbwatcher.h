/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Juan Giler <contact@example.org>
 */

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "tbparser.h"

/**
 * Result of one full profile scan.
 */
struct TbScanResult
{
    bool profileFound = false;
    QString profileRoot;    ///< directory that holds profiles.ini
    QString profilePath;    ///< active profile directory
    QString profileName;    ///< human readable profile name
    QString flatpakAppId;   ///< e.g. "org.mozilla.thunderbird" when sandboxed
    QVector<TbFolder> folders;
    QStringList warnings;
};
Q_DECLARE_METATYPE(TbScanResult)

/**
 * Background worker that performs all file system access for the application.
 *
 * The object lives in a dedicated QThread; every public slot must therefore be
 * invoked through a queued connection. No GUI class is touched from here.
 */
class ThunderbirdWatcher : public QObject
{
    Q_OBJECT

public:
    explicit ThunderbirdWatcher(QObject *parent = nullptr);
    ~ThunderbirdWatcher() override;

public Q_SLOTS:
    /// Starts the polling timer. Must run inside the worker thread.
    void start();
    /// Performs an immediate scan and emits scanFinished().
    void requestScan();
    /// Changes the polling interval; takes effect on the next start().
    void setPollInterval(int seconds);

Q_SIGNALS:
    void scanFinished(const TbScanResult &result);

private:
    void scan();
    void readFolderCache(const QString &profilePath, TbScanResult *result) const;

    QString discoverProfileRoot() const;
    QString resolveActiveProfile(const QString &root, QString *profileName) const;
    QString detectFlatpakAppId(const QString &root) const;

    class QTimer *m_timer = nullptr;
    int m_pollIntervalSeconds = 10;
};
