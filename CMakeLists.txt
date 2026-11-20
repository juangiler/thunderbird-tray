# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Juan Giler <contact@example.org>

cmake_minimum_required(VERSION 3.22)

project(thunderbird-tray
    VERSION 0.0.2
    DESCRIPTION "System tray companion for Thunderbird on Linux"
    HOMEPAGE_URL "https://github.com/juangiler/thunderbird-tray"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include(GNUInstallDirs)
include(CTest)

# Qt 6.6 is the first release that supports everything used here; Kirigami 6
# requires Qt 6.6 or newer as well.
find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Widgets Network DBus)
find_package(KF6Kirigami REQUIRED)

qt_standard_project_setup(REQUIRES 6.6)

qt_add_executable(thunderbird-tray
    src/main.cpp
    src/backend.cpp
    src/backend.h
    src/foldermodel.cpp
    src/foldermodel.h
    src/tbparser.cpp
    src/tbparser.h
    src/tbwatcher.cpp
    src/tbwatcher.h
    src/themecolors.cpp
    src/themecolors.h
    src/thunderbirdwindow.cpp
    src/thunderbirdwindow.h
    src/trayicon.cpp
    src/trayicon.h
)

# The QML files are placed at the root of the module so that they can be
# addressed with their plain name.
set_source_files_properties(src/qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
set_source_files_properties(src/qml/SettingsCard.qml PROPERTIES QT_RESOURCE_ALIAS SettingsCard.qml)

qt_add_qml_module(thunderbird-tray
    URI io.github.juangiler.thunderbirdtray
    VERSION 1.0
    QML_FILES
        src/qml/Main.qml
        src/qml/SettingsCard.qml
)

# Fallback icon, used when the icon theme does not provide a mail icon.
qt_add_resources(thunderbird-tray "app_icons"
    PREFIX "/"
    FILES
        resources/icons/thunderbird-tray.svg
)

target_link_libraries(thunderbird-tray PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    Qt6::Network
    Qt6::DBus
    KF6::Kirigami
)

set_target_properties(thunderbird-tray PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
)

install(TARGETS thunderbird-tray
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES resources/io.github.juangiler.thunderbird-tray.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(FILES resources/io.github.juangiler.thunderbird-tray.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
)

# The file name has to match the "Icon=" key of the desktop file.
install(FILES resources/icons/thunderbird-tray.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
    RENAME io.github.juangiler.thunderbird-tray.svg
)

# ---------------------------------------------------------------------------
# Tests
#
# The parsing of Thunderbird's profile files is pure logic, so it is tested
# without touching a real Thunderbird installation.
# ---------------------------------------------------------------------------
if(BUILD_TESTING)
    find_package(Qt6 6.6 REQUIRED COMPONENTS Test)

    qt_add_executable(tst_tbparser
        tests/tst_tbparser.cpp
        src/tbparser.cpp
        src/tbparser.h
    )

    target_link_libraries(tst_tbparser PRIVATE
        Qt6::Core
        Qt6::Test
    )

    target_include_directories(tst_tbparser PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

    add_test(NAME tst_tbparser COMMAND tst_tbparser)

    qt_add_executable(tst_foldermodel
        tests/tst_foldermodel.cpp
        src/foldermodel.cpp
        src/foldermodel.h
        src/tbparser.cpp
        src/tbparser.h
    )

    target_link_libraries(tst_foldermodel PRIVATE
        Qt6::Core
        Qt6::Test
    )

    target_include_directories(tst_foldermodel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

    add_test(NAME tst_foldermodel COMMAND tst_foldermodel)

    qt_add_executable(tst_themecolors
        tests/tst_themecolors.cpp
        src/themecolors.cpp
        src/themecolors.h
    )

    target_link_libraries(tst_themecolors PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
    )

    target_include_directories(tst_themecolors PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

    add_test(NAME tst_themecolors COMMAND tst_themecolors)

    qt_add_executable(tst_trayicon
        tests/tst_trayicon.cpp
        src/trayicon.cpp
        src/trayicon.h
        src/themecolors.cpp
        src/themecolors.h
    )

    target_link_libraries(tst_trayicon PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Test
    )

    target_include_directories(tst_trayicon PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

    # The fallback icon is needed when the icon theme has no mail icons.
    qt_add_resources(tst_trayicon "app_icons"
        PREFIX "/"
        FILES
            resources/icons/thunderbird-tray.svg
    )

    add_test(NAME tst_trayicon COMMAND tst_trayicon)

    # The tests do not need a display server.
    set_tests_properties(tst_trayicon PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    # Lint the QML files. This catches unknown properties and shadowed
    # properties, which would otherwise only show up at runtime.
    find_program(QMLLINT_EXECUTABLE NAMES qmllint-qt6 qmllint)
    if(QMLLINT_EXECUTABLE)
        add_test(NAME qmllint
            COMMAND ${QMLLINT_EXECUTABLE}
                    --unqualified info
                    --max-warnings 0
                    -I ${CMAKE_CURRENT_SOURCE_DIR}/src/qml
                    ${CMAKE_CURRENT_SOURCE_DIR}/src/qml/Main.qml
                    ${CMAKE_CURRENT_SOURCE_DIR}/src/qml/SettingsCard.qml
        )
    endif()
endif()
