/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Juan Giler <contact@example.org>
 */

#pragma once

#include <QObject>
#include <QString>

/**
 * Hides and restores the Thunderbird window.
 *
 * On Wayland no application may touch the window of another application,
 * so this asks KWin to do it with a short script. KWin offers exactly the
 * two things that are needed: a window can be minimized and it can be
 * activated again.
 *
 * The class registers a small object on the session bus; the script calls
 * back into it, so the application knows what actually happened.
 *
 * On a desktop without KWin the feature is simply not available.
 */
class ThunderbirdWindow : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.juangiler.ThunderbirdTray")

public:
    explicit ThunderbirdWindow(QObject *parent = nullptr);
    ~ThunderbirdWindow() override;

    /// True when the desktop can hide windows of other applications.
    static bool isSupported();

    /// True while the Thunderbird window is minimized.
    bool isHidden() const;

    /// The number of Thunderbird windows the desktop knows about.
    int windowCount() const;

Q_SIGNALS:
    /// Emitted after the desktop answered an action.
    void stateChanged();
    /// Asks the application to snooze notifications for @p minutes.
    void snoozeRequested(int minutes);

public Q_SLOTS:
    /// Asks the desktop to minimize the Thunderbird window.
    void hideWindow();

    /// Asks the desktop to restore and activate the Thunderbird window.
    void showWindow();

    /// Asks the desktop whether the Thunderbird window is minimized.
    void refreshState();

    /// Snoozes notifications for @p minutes; 0 cancels the snooze.
    void snooze(int minutes);

    /// Called by the script of the desktop; @p action is "hide", "restore"
    /// or "state", @p count the number of Thunderbird windows and @p hidden
    /// the number of minimized ones.
    void windowAction(const QString &action, int count, int hidden);

private:
    /// Runs a script on the desktop and waits for its answer.
    void runScript(const QString &script, const QString &action);

    int m_windowCount = 0;
    bool m_hidden = false;
};