/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Juan Giler <contact@example.org>
 */

#include "tbwatcher.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QTextStream>
#include <QThread>
#include <QTimer>

#include <utility>

ThunderbirdWatcher::ThunderbirdWatcher(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<TbScanResult>();
    qRegisterMetaType<TbFolder>();
}

ThunderbirdWatcher::~ThunderbirdWatcher() = default;

void ThunderbirdWatcher::start()
{
    if (!m_timer) {
        m_timer = new QTimer(this);
        m_timer->setInterval(m_pollIntervalSeconds * 1000);
        connect(m_timer, &QTimer::timeout, this, &ThunderbirdWatcher::scan);
        m_timer->start();
    }
    requestScan();
}

void ThunderbirdWatcher::requestScan()
{
    scan();
}

void ThunderbirdWatcher::setPollInterval(int seconds)
{
    const int clamped = qBound(2, seconds, 3600);
    if (m_pollIntervalSeconds == clamped) {
        return;
    }
    m_pollIntervalSeconds = clamped;
    if (m_timer) {
        m_timer->setInterval(m_pollIntervalSeconds * 1000);
    }
}

QString ThunderbirdWatcher::discoverProfileRoot() const
{
    const QString home = QDir::homePath();

    QStringList candidates;
    // Native installation.
    candidates << home + QStringLiteral("/.thunderbird");

    // Sandboxed installations (Flatpak, Snap, ...) redirect $HOME.
    const QString varApp = home + QStringLiteral("/.var/app");
    const QFileInfo varAppInfo(varApp);
    if (varAppInfo.isDir()) {
        const QStringList entries = QDir(varApp).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &entry : entries) {
            candidates << varApp + QLatin1Char('/') + entry + QStringLiteral("/.thunderbird");
        }
    }

    // Explicitly requested locations.
    candidates << home + QStringLiteral("/.var/app/org.mozilla.Thunderbird/.thunderbird");
    candidates << home + QStringLiteral("/.var/app/org.mozilla.Thunderbird");

    // Prefer a root that actually holds a usable profile.
    QString fallback;
    for (const QString &candidate : std::as_const(candidates)) {
        const QFileInfo info(candidate);
        if (!info.isDir() || !QFileInfo::exists(candidate + QStringLiteral("/profiles.ini"))) {
            continue;
        }
        if (fallback.isEmpty()) {
            fallback = candidate;
        }
        QString profileName;
        const QString profile = resolveActiveProfile(candidate, &profileName);
        if (!profile.isEmpty()
            && (QFileInfo::exists(profile + QStringLiteral("/folderCache.json"))
                || QFileInfo::exists(profile + QStringLiteral("/prefs.js")))) {
            return candidate;
        }
    }

    return fallback;
}

QString ThunderbirdWatcher::resolveActiveProfile(const QString &root, QString *profileName) const
{
    struct ProfileEntry
    {
        QString name;
        QString path;
        bool relative = true;
        bool isDefault = false;
    };

    QHash<QString, ProfileEntry> profiles; // keyed by path
    QStringList profileOrder;
    QStringList installDefaults;
    QString generalDefault;

    QFile file(root + QStringLiteral("/profiles.ini"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    QString section;
    QHash<QString, QString> values;
    const auto flushSection = [&]() {
        if (section.isEmpty()) {
            return;
        }
        if (section.startsWith(QLatin1String("Profile"))) {
            ProfileEntry entry;
            entry.name = values.value(QStringLiteral("Name"));
            entry.path = values.value(QStringLiteral("Path"));
            entry.relative = values.value(QStringLiteral("IsRelative"), QStringLiteral("1")) != QLatin1String("0");
            entry.isDefault = values.value(QStringLiteral("Default"), QStringLiteral("0")) == QLatin1String("1");
            if (!entry.path.isEmpty()) {
                profiles.insert(entry.path, entry);
                profileOrder.append(entry.path);
            }
        } else if (section.startsWith(QLatin1String("Install"))) {
            const QString def = values.value(QStringLiteral("Default"));
            if (!def.isEmpty()) {
                installDefaults.append(def);
            }
        } else if (section == QLatin1String("General")) {
            generalDefault = values.value(QStringLiteral("Default"));
        }
        values.clear();
    };

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char(';'))) {
            continue;
        }
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            flushSection();
            section = line.mid(1, line.size() - 2);
            continue;
        }
        const int equals = line.indexOf(QLatin1Char('='));
        if (equals <= 0) {
            continue;
        }
        values.insert(line.left(equals).trimmed(), line.mid(equals + 1).trimmed());
    }
    flushSection();

    if (profiles.isEmpty()) {
        return QString();
    }

    QString chosenPath;
    // 1. The most recently written [Install*] section marks the active profile.
    for (auto it = installDefaults.crbegin(); it != installDefaults.crend(); ++it) {
        if (profiles.contains(*it)) {
            chosenPath = *it;
            break;
        }
    }
    // 2. An explicit [General] Default.
    if (chosenPath.isEmpty() && !generalDefault.isEmpty() && profiles.contains(generalDefault)) {
        chosenPath = generalDefault;
    }
    // 3. A profile flagged with Default=1.
    if (chosenPath.isEmpty()) {
        for (const QString &path : std::as_const(profileOrder)) {
            if (profiles.value(path).isDefault) {
                chosenPath = path;
                break;
            }
        }
    }
    // 4. Prefer the release profile, then fall back to the first one.
    if (chosenPath.isEmpty()) {
        for (const QString &path : std::as_const(profileOrder)) {
            if (profiles.value(path).name.compare(QLatin1String("default-release"), Qt::CaseInsensitive) == 0) {
                chosenPath = path;
                break;
            }
        }
    }
    if (chosenPath.isEmpty()) {
        chosenPath = profileOrder.first();
    }

    const ProfileEntry entry = profiles.value(chosenPath);
    if (profileName) {
        *profileName = entry.name;
    }
    if (entry.relative) {
        return QDir(root).absoluteFilePath(entry.path);
    }
    return entry.path;
}

QString ThunderbirdWatcher::detectFlatpakAppId(const QString &root) const
{
    const QString marker = QStringLiteral("/.var/app/");
    const int index = root.lastIndexOf(marker);
    if (index < 0) {
        return QString();
    }
    QString tail = root.mid(index + marker.length());
    const int slash = tail.indexOf(QLatin1Char('/'));
    if (slash <= 0) {
        return QString();
    }
    return tail.left(slash);
}

void ThunderbirdWatcher::readFolderCache(const QString &profilePath, TbScanResult *result) const
{
    const QString cachePath = profilePath + QStringLiteral("/folderCache.json");

    QFile file(cachePath);
    if (!file.open(QIODevice::ReadOnly)) {
        result->warnings << tr("Thunderbird has not written folderCache.json yet. "
                               "Counts appear after the first mail check.");
        return;
    }

    const QByteArray content = file.readAll();
    const QHash<QString, QString> prefs = TbParser::readPrefsFile(profilePath + QStringLiteral("/prefs.js"));

    result->folders = TbParser::parseFolderCache(content, prefs, profilePath, &result->warnings);
}

void ThunderbirdWatcher::scan()
{
    // The application is shutting down: do not start another scan.
    if (QThread::currentThread()->isInterruptionRequested()) {
        return;
    }

    TbScanResult result;

    const QString root = discoverProfileRoot();
    if (root.isEmpty()) {
        result.warnings << tr("No Thunderbird profile was found in %1 or %2.")
                            .arg(QDir::homePath() + QStringLiteral("/.thunderbird"),
                                 QDir::homePath() + QStringLiteral("/.var/app"));
        Q_EMIT scanFinished(result);
        return;
    }

    result.profileRoot = root;
    result.flatpakAppId = detectFlatpakAppId(root);

    QString profileName;
    const QString profile = resolveActiveProfile(root, &profileName);
    if (profile.isEmpty()) {
        result.warnings << tr("profiles.ini does not contain a usable profile.");
        Q_EMIT scanFinished(result);
        return;
    }

    result.profileFound = true;
    result.profilePath = profile;
    result.profileName = profileName;

    readFolderCache(profile, &result);

    Q_EMIT scanFinished(result);
}
