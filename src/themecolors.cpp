/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Juan Giler <contact@example.org>
 */

#include "themecolors.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QPalette>
#include <QStandardPaths>
#include <QTextStream>

namespace
{
/// Reads a single value from an INI file, e.g. "[General] AccentColor=1,2,3".
QString readIniValue(const QString &path, const QString &group, const QString &key)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    QString currentGroup;
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')) || line.startsWith(QLatin1Char(';'))) {
            continue;
        }
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            currentGroup = line.mid(1, line.size() - 2);
            continue;
        }
        if (currentGroup != group) {
            continue;
        }
        const int equals = line.indexOf(QLatin1Char('='));
        if (equals <= 0) {
            continue;
        }
        if (line.left(equals).trimmed() == key) {
            return line.mid(equals + 1).trimmed();
        }
    }

    return QString();
}
} // namespace

namespace ThemeColors
{

QColor parseColor(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return QColor();
    }

    if (trimmed.startsWith(QLatin1Char('#'))) {
        return QColor(trimmed);
    }

    const QStringList parts = trimmed.split(QLatin1Char(','));
    if (parts.size() < 3) {
        return QColor();
    }

    bool okR = false;
    bool okG = false;
    bool okB = false;
    const int red = parts.at(0).trimmed().toInt(&okR);
    const int green = parts.at(1).trimmed().toInt(&okG);
    const int blue = parts.at(2).trimmed().toInt(&okB);
    if (!okR || !okG || !okB) {
        return QColor();
    }

    return QColor(red, green, blue);
}

QColor fallbackAccentColor()
{
    // The classic KDE blue.
    return QColor(0x3d, 0xae, 0xe9);
}

QColor counterColor(const QColor &color, bool darkTheme)
{
    if (color.isValid()) {
        return color;
    }
    // White on dark themes, black on light ones.
    return darkTheme ? QColor(Qt::white) : QColor(Qt::black);
}

QString dominantAccount(const QMap<QString, int> &unreadPerAccount,
                        const QHash<QString, QColor> &colors)
{
    QString bestAccount;
    int bestUnread = 0;

    for (auto it = unreadPerAccount.constBegin(); it != unreadPerAccount.constEnd(); ++it) {
        if (it.value() <= 0) {
            continue;
        }
        const QColor color = colors.value(it.key());
        if (!color.isValid()) {
            // The account has no colour of its own and does not compete.
            continue;
        }
        if (it.value() > bestUnread) {
            bestUnread = it.value();
            bestAccount = it.key();
        }
    }

    return bestAccount;
}

QColor dominantAccountColor(const QMap<QString, int> &unreadPerAccount,
                            const QHash<QString, QColor> &colors,
                            const QColor &fallback)
{
    const QString account = dominantAccount(unreadPerAccount, colors);
    if (account.isEmpty()) {
        return fallback;
    }
    return colors.value(account);
}

Theme read(const QString &path)
{
    Theme theme;

    QString file = path;
    if (file.isEmpty()) {
        file = QStandardPaths::locate(QStandardPaths::GenericConfigLocation, QStringLiteral("kdeglobals"));
    }
    if (file.isEmpty()) {
        file = QDir::homePath() + QStringLiteral("/.config/kdeglobals");
    }

    theme.accentColor = parseColor(readIniValue(file, QStringLiteral("General"), QStringLiteral("AccentColor")));
    theme.backgroundColor =
        parseColor(readIniValue(file, QStringLiteral("Colors:View"), QStringLiteral("BackgroundNormal")));

    if (theme.backgroundColor.isValid()) {
        theme.darkTheme = theme.backgroundColor.lightness() < 128;
    } else if (QGuiApplication::instance()) {
        const QColor window = QGuiApplication::palette().color(QPalette::Window);
        theme.darkTheme = window.lightness() < 128;
    }

    return theme;
}

} // namespace ThemeColors
