# SPDX-FileCopyrightText: 2026 Plasma Tabbed Notes contributors
# SPDX-License-Identifier: BSD-2-Clause
cmake_minimum_required(VERSION 3.22)
project(plasma-sticky-note-tabs VERSION 0.3.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(KDE_COMPILERSETTINGS_LEVEL 6.14)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

find_package(ECM 6.26 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs6)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 6.10 REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(KF6 6.26 REQUIRED COMPONENTS Config CoreAddons I18n Svg)
find_package(Plasma 6.7 REQUIRED)

add_subdirectory(applets/notes)
ki18n_install(po)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.10 REQUIRED COMPONENTS Test Widgets)
    find_package(PlasmaQuick 6.7 REQUIRED)
    add_subdirectory(tests)
endif()

install(FILES README.md LICENSE.md UPSTREAM DESTINATION ${KDE_INSTALL_DATADIR}/doc/plasma-sticky-note-tabs)
install(FILES res/Preview.png res/Preview.png.license DESTINATION ${KDE_INSTALL_DATADIR}/doc/plasma-sticky-note-tabs/res)
install(DIRECTORY LICENSES DESTINATION ${KDE_INSTALL_DATADIR}/licenses/plasma-sticky-note-tabs)
