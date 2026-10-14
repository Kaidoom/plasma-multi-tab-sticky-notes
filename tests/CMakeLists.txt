# SPDX-FileCopyrightText: 2026 Plasma Tabbed Notes contributors
# SPDX-License-Identifier: BSD-2-Clause
add_executable(applet-integration appletintegration.cpp)
target_link_libraries(applet-integration PRIVATE
    Qt6::Test Qt6::Widgets Qt6::Quick Plasma::Plasma Plasma::PlasmaQuick
    KF6::ConfigCore KF6::ConfigQml KF6::Package)
add_dependencies(applet-integration dev.kaidoom.tabbednotes)
add_test(NAME applet-integration COMMAND applet-integration)
set_tests_properties(applet-integration PROPERTIES
    ENVIRONMENT "QT_PLUGIN_PATH=${CMAKE_BINARY_DIR}/bin;QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")

add_executable(storage-test storage.cpp
    ../applets/notes/note.cpp ../applets/notes/abstractnoteloader.cpp
    ../applets/notes/filesystemnoteloader.cpp ../applets/notes/notemanager.cpp)
target_include_directories(storage-test PRIVATE ../applets/notes)
target_link_libraries(storage-test PRIVATE Qt6::Test Qt6::Qml KF6::CoreAddons KF6::I18n)
add_test(NAME storage COMMAND storage-test)
