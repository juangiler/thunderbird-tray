/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Juan Giler <contact@example.org>
 */

#pragma once

#include <QColor>
#include <QHash>
#include <QMap>
#include <QString>

/**
 * Reads the colors of the current KDE Plasma theme.
 *
 * The values come from `kdeglobals`, the configuration file of the desktop
 * theme. They are read on demand, because the theme (and with it the
 * accent color) can change while the application is running.
 */
namespace ThemeColors
{
/// The colors of the current theme.
struct Theme
{
    QColor accentColor;     ///< invalid when the theme defines none
    QColor backgroundColor; ///< invalid when it cannot be determined
    bool darkTheme = false; ///< true for dark themes
};

/**
 * Parses a color as it is written in kdeglobals.
 *
 * Both the "102,83,26" form and the "#66351a" form are understood.
 */
QColor parseColor(const QString &value);

/// The KDE blue, used when the theme has no accent color.
QColor fallbackAccentColor();

/**
 * The color of the number on the badge.
 *
 * @param color      the color the user picked; invalid means "automatic"
 * @param darkTheme  whether the desktop theme is a dark one
 *
 * An automatic color is white on dark themes and black on light ones.
 */
QColor counterColor(const QColor &color, bool darkTheme);

/**
 * Picks the account that speaks for the badge.
 *
 * There is only one badge, so a single account has to speak for all of
 * them: the account with the most unread messages wins. Accounts without a
 * colour of their own do not take part in the decision.
 *
 * @param unreadPerAccount the unread messages of every monitored account
 * @param colors           the colour of every account that has one
 *
 * @return the name of the account, empty when no account qualifies
 */
QString dominantAccount(const QMap<QString, int> &unreadPerAccount,
                        const QHash<QString, QColor> &colors);

/**
 * The color of the badge when every account has its own color.
 *
 * @see dominantAccount
 */
QColor dominantAccountColor(const QMap<QString, int> &unreadPerAccount,
                            const QHash<QString, QColor> &colors,
                            const QColor &fallback);

/**
 * Reads the theme colors from `kdeglobals`.
 *
 * @param path the path of the file; the default location is used when empty
 */
Theme read(const QString &path = QString());

} // namespace ThemeColors
