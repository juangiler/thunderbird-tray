# AGEND.md — Thunderbird Tray

**Status:** v1.0.0 implemented, building and verified on the reference machine.
**Project:** `/var/home/juangiler/Projects/thunderbird-tray`
**Last updated:** 2026-11-19

---

## 1. Intent

A system tray companion for Mozilla Thunderbird on Linux. It lives in the
notification area, shows how many unread messages the monitored mail folders
contain, flashes when new mail arrives, and gives one-click access to
Thunderbird itself, a manual mail check and the settings.

The application is a *companion*, not a mail client. It never renders a
message, never talks to an IMAP server and never modifies the profile. It
only reads the state Thunderbird already maintains on disk and surfaces it.

**Why it exists:** Thunderbird's own tray integration is minimal (no badge,
no per-account counters, no animation) and it disappears entirely on
GNOME-derived desktops that do not ship the Thunderbird extension.

---

## 2. Goals

| # | Goal | Status |
|---|------|--------|
| G1 | Native system tray icon with an unread badge | done |
| G2 | Animated (blinking) icon while unread mail is pending | done |
| G3 | Context menu: Open Thunderbird / Check Mail Now / Settings / Quit | done |
| G4 | Left-click activates Thunderbird | done |
| G5 | Automatic Thunderbird profile detection (native + Flatpak + Snap) | done |
| G6 | Unread counter computed **off** the UI thread | done |
| G7 | Per-folder monitoring, configurable | done |
| G8 | Kirigami 6 settings window (General / Animation / Accounts) | done |
| G9 | Launch at login | done |
| G10 | Single instance | done |
| G11 | Builds cleanly with `cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug` | done |
| G12 | Unit tested parsing logic | done |
| G13 | Gradual fade animation instead of a hard blink | done |
| G14 | Customizable badge color, optionally following the system accent color | done |
| G15 | Customizable counter font (family and size) and color, automatic per theme | done |

## 3. Non-goals

* Not a mail client; no message list, no composer, no search.
* No server-side polling. Thunderbird stays the only thing that talks to
  the mail servers (see §8, limitation L1).
* No support for the Thunderbird Lightning API — it was removed in
  Thunderbird 115 and never existed in a stable, documented form.
* No GTK/Qt4/Qt5 fallback; this targets Qt 6 and Plasma 6 only.
* No packaging (RPM/Flatpak) in v1.0 — the CMake install rules and the
  AppStream metadata are in place, packaging is a separate task (T11).

---

## 4. Constraints

| Constraint | Consequence |
|------------|-------------|
| Pure C++20, no Python/PyQt/PySide | All logic is in C++; QML only for the settings UI |
| CMake ≥ 3.22, Qt 6, KF6 Kirigami 6 | `find_package(Qt6 6.6)`, `find_package(KF6Kirigami)` |
| Must run on Fedora/Bazzite **and** on KDE Plasma 6.6+ elsewhere | No distro-specific APIs, no hardcoded paths |
| Wayland must work | No X11-only focus stealing, no `xdotool` |
| Thunderbird's internals are undocumented | Every file format we read is verified against a real profile |
| Thunderbird may not be running | All data must be readable from disk without a running client |

---

## 5. Stack review — is this the modern gold standard?

### Verdict

**Yes.** Qt 6 + C++20 + CMake + KDE Frameworks 6 (Kirigami 6) is the correct
and current stack for a native Linux desktop application in 2026. There is no
better-supported combination for a KDE-targeted app, and nothing in the
required feature set (native tray icon, background thread, native settings
UI) needs a different technology.

For reference, the alternatives were considered and rejected:

| Alternative | Why not |
|-------------|---------|
| Python + PyQt6/PySide6 | Explicitly out of scope. Also: no Kirigami, packaging via PyInstaller, and the spec asked for native C++. |
| GTK4 / libadwaita | Not the KDE look; Kirigami was required. |
| Pure QML application (no C++) | The tray icon needs `QSystemTrayIcon`, which is C++ only. |
| `KStatusNotifierItem` (KF6) | The "purist" KDE way to do a tray icon (SNI protocol, survives tray restarts). **Not installed on this system** and not universally available, so `QSystemTrayIcon` was kept per spec. This is the one item worth revisiting — see T10. |
| Rust + `cosmic`/`iced` | Would abandon Kirigami and QML entirely; no benefit for a settings dialog. |

### Corrections to the original specification

The spec as written contained three factual errors that would have caused
build or runtime failures. All three are corrected in the implementation:

1. **The module list was incomplete.**
   `QSystemTrayIcon` and `QMenu` live in **Qt Widgets**, not in QtGui.
   A `QGuiApplication` therefore cannot provide the tray context menu that
   the spec requires. Fix: the app uses **`QApplication`** and links
   `Qt6::Widgets`.
   *(A second, smaller gap: `QLocalServer` for the single-instance guard
   lives in **Qt Network**, so `Qt6::Network` is linked as well.)*

2. **`FormCard` is not `Kirigami.FormCard` any more.**
   Since Kirigami 6 the form card delegates were moved out of the Kirigami
   module into the separate QML module
   `org.kde.kirigamiaddons.formcard` (shipped by `kf6-kirigami-addons`).
   The QML therefore uses:
   ```qml
   import org.kde.kirigamiaddons.formcard as FormCard
   ```
   Note that this module is **runtime-only** — there is no
   `KF6KirigamiAddons` CMake package on this system, so the build links
   only `KF6::Kirigami` and the QML engine resolves the FormCard module
   dynamically at startup.

3. **The assumed data source no longer exists.**
   The spec assumed `panacea.dat` and a SQLite database. On Thunderbird 160
   (the version actually installed here) `panacea.dat` is **gone** and
   `global-messages-db.sqlite` is **locked** while Thunderbird runs, so it
   cannot be read reliably. See §6 for what is used instead and why.

### The reference environment

| Component | Version |
|-----------|---------|
| OS | Bazzite 45.20261117.1 (Kinoite / Fedora base) |
| Qt | 6.11.2 |
| KDE Frameworks 6 | 6.32.0 (Kirigami 6.32.0) |
| kirigami-addons | 1.13.1 (provides the FormCard module) |
| Compiler | GCC 16.2.1 |
| CMake / Ninja | 4.3.0 / 1.13.2 |
| Session | Wayland (KWin) + XWayland |
| Thunderbird | 160 (Flatpak `org.mozilla.thunderbird`) |

**Portability note.** Kirigami 6.0 required Qt 6.6, but the *installed*
Kirigami 6.32 requires **Qt 6.10** (`find_dependency(Qt6Core 6.10.0)` in
`KF6KirigamiConfig.cmake`). The project therefore declares a conservative
`find_package(Qt6 6.6 REQUIRED ...)`; the real minimum on any given distro is
whatever its Kirigami package demands. Building against Plasma 6.6–6.9
distros works because those ship Kirigami 6.0–6.x with Qt 6.6–6.9.

---

## 6. Where the unread counts come from

This was the hardest part of the project and the place where the original
spec was most wrong.

### What was tried and rejected

| Source | Verdict |
|--------|---------|
| `panacea.dat` | **Does not exist** in Thunderbird 115+. Removed. |
| `global-messages-db.sqlite` | Exists but is **locked by a running Thunderbird**; reading it needs `Qt6::Sql` and fails whenever the mail client is open — which is exactly when it matters. |
| `.msf` index files | Text-Mork format, and they only hold *retained* headers (109 of 622 messages in the reference profile). A lower bound, not a total. |
| **Mail server queries (IMAP)** | Not possible: Thunderbird exposes no public API to ask it to fetch, and re-implementing IMAP is out of scope. |

### What is used

**`folderCache.json`** — the authoritative per-folder summary that
Thunderbird 115+ writes into the profile directory. For the reference
profile it contains 37 folders with `totalUnreadMsgs`, `totalMsgs` and
`serverUnseen` per folder. It is plain JSON, needs no extra dependency, and
is readable while Thunderbird is running.

The account labels come from **`prefs.js`**:

```
mail.accountmanager.accounts      →  account1,account3,account4,account2
mail.account.accountN.server       →  serverN
mail.account.accountN.identities   →  idN
mail.identity.idN.useremail        →  juangiler@icloud.com
mail.server.serverN.hostname       →  imap.mail.me.com
mail.server.serverN.name           →  juangiler@icloud.com
```

The active profile is resolved from **`profiles.ini`** with the priority
`[Install*].Default` (last one wins) → `[General].Default` → `Default=1` →
`default-release` → first entry.

### Verified against the reference profile

```
profile path  : ~/.var/app/org.mozilla.thunderbird/.thunderbird/rcifbucd.default-release
profile name  : default-release
folders       : 37
unread total  : 6788   (533 gmail + 1717 icloud + 4538 outlook — INBOXes only)
```

Virtual/aggregate folders (`Todos`, `All Mail`, `Unified`) mirror the
content of other folders and would double count, so they are excluded from
the automatic default selection.

---

## 7. Architecture

### Thread model

```
  GUI thread                              worker thread
  ┌───────────────────────────┐            ┌────────────────────────┐
  │ main.cpp    QApplication  │            │ ThunderbirdWatcher     │
  │ backend.cpp AppBackend    │            │  QTimer (poll)         │
  │ trayicon.cpp TrayIcon     │            │  scan()                │
  │ foldermodel.cpp           │            │   └─ TbParser (pure)   │
  │ QML: Main.qml            │            └────────────────────────┘
  └───────────────────────────┘                   ▲
             │                                     │ queued signal
             └──────── scanFinished(TbScanResult) ─┘
```

* All file system access happens on the worker thread. The GUI thread never
  blocks on I/O.
* `TbScanResult` is a registered metatype, so it can cross the thread
  boundary through a queued connection.
* `TbParser` is deliberately free of Qt GUI classes so that it can be unit
  tested without a display (see `tests/tst_tbparser.cpp`).

### Component map

| File | Responsibility |
|------|----------------|
| `src/main.cpp` | `QApplication`, single-instance guard, QML engine, CLI options |
| `src/backend.{h,cpp}` | `AppBackend`: settings, aggregation, autostart file, Thunderbird launch, the QML-facing API |
| `src/tbwatcher.{h,cpp}` | `ThunderbirdWatcher`: profile discovery, `profiles.ini` parsing, polling, runs in a `QThread` |
| `src/tbparser.{h,cpp}` | `TbParser`: pure parsing of `prefs.js` and `folderCache.json` (unit tested) |
| `src/foldermodel.{h,cpp}` | `FolderListModel`: flat list model of folders for QML, holds the enabled state |
| `src/trayicon.{h,cpp}` | `TrayIcon`: `QSystemTrayIcon`, badge painting, fade animation, context menu |
| `src/themecolors.{h,cpp}` | Reads the accent color and the theme brightness from `kdeglobals` |
| `src/qml/Main.qml` | `Kirigami.ApplicationWindow`, hidden by default, closes to tray |
| `src/qml/SettingsCard.qml` | All settings as `FormCard`s |
| `tests/tst_tbparser.cpp` | Regression tests for the parsing logic |
| `tests/tst_foldermodel.cpp` | Tests for the list model, the counters and the monitoring state |
| `tests/tst_themecolors.cpp` | Tests for the theme color parsing |
| `tests/tst_trayicon.cpp` | Tests for the badge: colors, the counter and the fade |

### QML API surface

`AppBackend` exposes the state to QML through `Q_PROPERTY` / `Q_INVOKABLE` /
`Q_SLOT` as required:

```
unreadCount  monitoredFolders  totalMessages  newMail  statusText
profilePath  profileName  profileFound  warnings  thunderbirdRunning
launchAtBoot  minimizeToTray  animateIcon  animationInterval  pollInterval
folders (QAbstractListModel*)

openThunderbird()  checkMailNow()  openSettings()  quitApplication()
rescan()  setFolderEnabled(id, enabled)  isFolderEnabled(id)  hideToTray()
```

---

### 7.1 Appearance of the tray icon

The badge is composed at runtime, so every part of it can be adapted:

| Setting | Default | Source |
|---------|---------|--------|
| Badge color | the accent color of the desktop theme | `[General] AccentColor` in `kdeglobals` |
| Badge color per account | none | user setting, one color per account |
| Counter color | white on dark themes, black on light ones | `[Colors:View] BackgroundNormal` in `kdeglobals` |
| Counter font family | the system font | user setting |
| Counter font size | scaled with the icon (64 px icon → 23 px) | user setting |
| Animation | a smooth fade between fully opaque and 23 % | user setting |

The theme colors are re-read every ten seconds, so switching the Plasma
theme or the accent color is picked up without a restart.

The animation runs at 30 frames per second. The "blink interval" setting is
the time one half of the fade takes, so the badge breathes smoothly instead
of switching between two states.

### 7.2 Per account colors

There is only **one** badge, so the accounts cannot each have a badge of
their own. The rule is simple and predictable:

> Among the monitored accounts that have a color of their own, the one with
> the **most unread messages** decides the color of the badge.

* Accounts without a color of their own do not compete; they use the global
  badge color (the accent color or the one the user picked).
* An account with unread messages of zero never wins, even if it has a
  color.
* The number on the badge is always the total of all monitored folders.

The alternative — blending the colors of all accounts with unread mail —
was rejected because the result is a color that belongs to no account and
therefore tells the user nothing.

Birdtray solves the same problem differently: it creates one tray icon per
account. That would be a much bigger change here, because it also means one
`QSystemTrayIcon` per account, a per icon menu and a per icon unread
counter — see T28.

### 7.3 Feature comparison with Birdtray

| Birdtray feature | Status here |
|------------------|-------------|
| Unread counter in the tray icon | ✅ done |
| Animated icon for new mail | ✅ done (a gradual fade) |
| Snooze new mail notifications | ✅ done (configurable duration via tray menu/settings/DBus; in-memory) |
| Reads Thunderbird's own data, needs no extension | ✅ done (`folderCache.json`) |
| Hide and restore the Thunderbird window, context menu | ✅ done on KDE Plasma/KWin (Wayland) via KWin scripting; single tray icon |
| Choose which accounts are checked | ✅ done (per folder) |
| Different colors per account | ✅ done: one badge, the account with the most unread mail decides its color (§7.2) |
| Launch Thunderbird at start, terminate it on quit | ⚠️ partially: optional "launch Thunderbird when Thunderbird Tray starts" setting added; terminate-on-quit not implemented |
| Indicate that Thunderbird is running | ✅ done (green dot in the bottom-left corner of the tray icon; menu/actions also reflect state) |
| Configurable "new email" templates | ❌ out of scope: that is a mail client feature, not a tray companion |

## 8. Known limitations

| # | Limitation | Mitigation / status |
|---|-----------|---------------------|
| L1 | **"Check Mail Now" cannot force a server round trip.** Thunderbird has no public API for it. | The action rescans the local counters immediately and starts Thunderbird if it is not running (which checks mail on startup). Documented in the UI: *"Refreshes the unread counters immediately."* |
| L2 | **Wayland focus.** A foreign window cannot be raised programmatically. | Thunderbird raises its own window when it is started. On X11 the same path works. |
| L3 | Counts are only as fresh as Thunderbird's own cache. | Poll interval is configurable (2–300 s, default 10 s). |
| L4 | `folderCache.json` does not exist until Thunderbird has run at least once. | A warning is shown: *"Thunderbird has not written folderCache.json yet."* |
| L5 | Virtual folders are detected by **name** (`Todos`, `All Mail`, `Unified`, `Smart Mailboxes`). A user folder with such a name would be treated as virtual. | Documented heuristic; the folder can still be enabled manually. |
| L6 | The account label falls back to the hostname when an account has no identity (e.g. Local Folders). | Intentional. |
| L7 | CMake prints 20 identical warnings while configuring (one per QML plugin dependency, for both targets): `componentsplugin`, `dateandtimeplugin`, `delegatesplugin`, `formcardplugin`, `iconthemesplugin`, `itemmodelsplugin`, `kcoreaddonsplugin`, `qqc2desktopstyleplugin`, `treeviewplugin` have no CMake package. | Benign — they are dynamic runtime plugins, resolved by the QML engine, not linked. |

---

## 9. Agenda

### Done

| # | Task | Result |
|---|------|--------|
| T1 | Research Thunderbird's on-disk format against a live TB 160 profile | `folderCache.json` chosen; `panacea.dat`/SQLite rejected (§6) |
| T2 | Project skeleton + CMake | `CMakeLists.txt`, Qt6 + KF6Kirigami, install rules |
| T3 | Profile discovery | native, Flatpak (`~/.var/app/*`), Snap layouts; `profiles.ini` resolution |
| T4 | Parser | `prefs.js` → accounts/identities; `folderCache.json` → folders/counters |
| T5 | Background worker | `ThunderbirdWatcher` in a `QThread`, queued `scanFinished` |
| T6 | Tray icon | `QSystemTrayIcon`, badge painted at runtime, blink animation, context menu |
| T7 | Backend | `AppBackend` with the full `Q_PROPERTY`/`Q_INVOKABLE` surface, `QSettings` persistence |
| T8 | QML UI | `Main.qml` (Kirigami.ApplicationWindow) + `SettingsCard.qml` (FormCard) |
| T9 | Launch at login | writes `~/.config/autostart/io.github.juangiler.thunderbird-tray.desktop` |
| T10 | Single instance | `QLocalServer`/`QLocalSocket`; a second instance asks the first to show itself |
| T11 | Unit tests for the parser | `tests/tst_tbparser.cpp`, 20 cases, wired into `ctest` |
| T12 | Bug fix: account names resolved to `"l"` | **Root cause:** the `"/ImapMail/"` marker was treated as 8 characters long (it is 10). Fixed by deriving the prefix length from the marker itself; covered by a regression test. |
| T13 | Documentation | this file, plus `README.md` |
| T14 | Independent code review + fixes | 13 findings that were real, all verified against the sources and fixed (see §9.1) |
| T15 | Unit tests for the folder model | `tests/tst_foldermodel.cpp`, 10 cases, wired into `ctest` |
| T16 | QML lint gate | `qmllint` runs as part of `ctest`; it catches unknown and shadowed QML properties |
| T17 | Appearance options | badge color (with a "follow the system accent color" option), counter color, counter font and size, gradual fade animation |
| T18 | Theme detection | reads the accent color and the theme brightness from `kdeglobals` (`src/themecolors.cpp`), re-read every ten seconds |
| T19 | Per account badge colors | one color per account; the account with the most unread mail decides the color of the single badge (§7.2) |
| T20 | Snooze notifications (T19 open item) | `snooze(minutes)` suppresses the new-mail animation/hint and updates status/tooltip; tray menu (5/15/30/60 min + cancel), settings control, DBus-callable, in-memory timer. |
| T21 | Hide/restore Thunderbird window (T20 open item, single icon) | `ThunderbirdWindow` asks KWin via `/Scripting` (`loadScript`/`start`) to minimize/restore+activate matching Thunderbird windows and reports state back over session DBus; tray menu and settings actions, state refreshed on menu show. |
| T23 | Thunderbird-running indicator in tray icon | Green status dot drawn in the bottom-left corner when Thunderbird is running; coexists with the unread badge; menu/actions reflect state. |

### Open

| # | Task | Notes |
|---|------|-------|
| T22 | Package as Flatpak / RPM | Metadata (`appdata`, `.desktop`) is already in `resources/`; the packaging manifests are missing. |
| T23 | Switch to `KStatusNotifierItem` | Strictly better on KDE (survives a tray restart, native badge support), but the KF6 package is not available here. |
| T24 | One tray icon per account | Birdtray does this, so every account can have its own icon, color and counter. It means one `QSystemTrayIcon` per account with its own menu. (User preferred single icon for T20.) |
| T25 | Terminate Thunderbird on quit (optional) | "Launch Thunderbird when Thunderbird Tray starts" is implemented (General setting, off by default); terminating Thunderbird when the tray quits is still open. |
| T27 | Translations | The code uses `qsTr()`/`tr()`, but no `.ts` files are generated yet. Note: the `i18n()` QML function is **not** available in this build — only `qsTr`/`qsTranslate`/`qsTrId` are. |
| T28 | Icon theme integration | The tray icon is composed at runtime from Breeze mail icons; a dedicated themed SVG badge is a possible refinement. |
| T29 | Settings window polish | Account selection is currently a flat list of all folders; grouping by account would be nicer. |
| T30 | Tests for `profiles.ini` parsing | `resolveActiveProfile()` and `discoverProfileRoot()` are still untested; they need a temporary profile directory to test properly. |

### 9.1 Findings of the code review (T14)

Every finding was verified against the sources (and, where possible, the
installed Qt/Kirigami sources) before being acted on. The ones that were
real:

| # | Finding | Fix |
|---|---------|-----|
| 1 | **The tray icon was never shown.** `TrayIcon::show()` had no caller, so the process was completely invisible on a desktop *with* a working tray. | `AppBackend` calls `m_tray->show()`. Verified end to end: the icon is now registered with the desktop's `StatusNotifierWatcher`. |
| 2 | **The single-instance "show" request was dropped.** `QLocalServer::newConnection` was never connected. | Wired up; a second instance now makes the running one show its window. |
| 3 | **Unchecking a folder did not update the counter.** The totals were only recomputed on the next poll. | The aggregation moved into `FolderListModel::totals()` and `AppBackend::recalculate()`, which is called from both the scan and the toggle. Verified: 6788 → 6255 → 6788 immediately. |
| 4 | **The role `enabled` shadowed `Item.enabled`** in the QML delegate, which disabled the row itself — an unchecked folder could never be re-enabled. | The role is now called `monitored`. `qmllint` flags this class of mistake, and linting is part of `ctest` now. |
| 5 | **`Kirigami.MessageType.Info` does not exist** (`Information`, `Positive`, `Warning`, `Error`). | Corrected to `Kirigami.MessageType.Information`. |
| 6 | **`qsTr()` does not substitute `%1`/`%2`** — the counts were passed as the disambiguation argument and the UI showed the raw placeholder. | `qsTr("…").arg(a).arg(b)`. |
| 7 | **The `QThread` could be destroyed while running** (the `wait()` result was ignored). | `requestInterruption()` + `quit()` + a bounded `wait()` with `terminate()` as a last resort; the worker aborts a scan when an interruption is requested. |
| 8 | **The single-instance guard could unlink a live server's socket.** `QLocalServer::setSocketOptions(UserAccessOption)` switches Qt to the *WorldAccessOption* implementation, which binds a temporary socket and **renames it over the target** — so `listen()` silently succeeded and two instances ran side by side. | The socket options are left at their default so that `bind()` fails with `EADDRINUSE`; the socket is then restricted to the owner with `QFile::setPermissions()`. The socket name contains the uid, so instances of different users cannot collide. |
| 9 | The section text about the folder count had inverted visibility. | Corrected. |
| 10 | The model was reset on **every** poll, recreating every QML delegate and making the list flicker. | `setFolders()` now updates the existing rows in place and only resets when the set of folders really changed. |
| 11 | `setFolderEnabled()` stored ids of folders that do not exist. | Unknown ids are rejected and not persisted. |
| 12 | The `qobject_cast` in `--dump-state` was used without a null check. | Checked. |
| 13 | Smaller ones: `Exec=%1` was not quoted (breaks on paths with spaces), `QProcess::startDetached()` failures were silent, the "Thunderbird is running" flag was wrong for the first five seconds, the context menu was deleted before the icon that referenced it, and dead code (`hideToTray()`, `trayIcon()`, `stop()`, `pollIntervalChanged`) was removed. | All fixed. |

Findings that were **not** defects and were left alone: the cross-thread
metatype registration, all connection types, the `deleteLater` idiom, the
`QHash` iterator invalidation, the `Qt 6` API usage and the FormCard API usage.

---

## 10. Verification

```bash
cd ~/Projects/thunderbird-tray
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/thunderbird-tray --dump-state
```

| Check | Result |
|-------|--------|
| Configure | exit 0; 20 identical benign QML-plugin warnings (see L7) |
| Build | exit 0, no errors, **no compiler warnings** |
| Unit tests | 49/49 cases pass (`ctest`: `tst_tbparser` 20, `tst_foldermodel` 10, `tst_themecolors` 10, `tst_trayicon` 9) |
| QML lint | `qmllint` passes as part of `ctest` |
| Runtime on Wayland/KWin | starts clean, no QML errors, tray icon present |
| Tray icon | registered with the desktop's `StatusNotifierWatcher` (verified over DBus, the item belongs to our process) |
| Single instance | a second launch exits 0 and makes the running instance show its window |
| Profile detection | `default-release` found, 37 folders |
| Unread counts | 6788 — matches `folderCache.json` exactly |
| Account labels | gmail / icloud / outlook / Local Folders all resolve correctly |
| Toggling a folder | the counter changes immediately (6788 → 6255 → 6788), no wait for the next poll |
| Theme detection | accent color `#66531a` and "dark theme" read from `kdeglobals` |
| Badge rendering | verified pixel by pixel in `tst_trayicon` (color, counter color, fade) |
| `cmake --install` | installs binary, desktop file, icon and AppStream metadata |
| `desktop-file-validate` | OK |
| `appstreamcli validate` | OK apart from `url-not-reachable` (the GitHub repository does not exist yet) |

`--dump-state` is a diagnostic mode that prints the detected state and exits;
`--settings` opens the settings window on start.
