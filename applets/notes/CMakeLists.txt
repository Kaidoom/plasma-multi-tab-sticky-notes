add_definitions(-DTRANSLATION_DOMAIN="plasma_applet_dev.kaidoom.tabbednotes")

plasma_add_applet(dev.kaidoom.tabbednotes GENERATE_APPLET_CLASS
    QML_SOURCES
        qml/main.qml
        qml/NoteEditor.qml
        qml/TabBar.qml
        qml/ShortcutMenuItem.qml
        qml/configAppearance.qml
        qml/config.qml
    CPP_SOURCES
        abstractnoteloader.cpp
        documenthandler.cpp
        filesystemnoteloader.cpp
        note.cpp
        noteiconengine.cpp
        notemanager.cpp
        tabmodel.cpp
        noteshelper.cpp
    RESOURCES
        main.xml
)

target_link_libraries(dev.kaidoom.tabbednotes PRIVATE
    Qt::Quick
    KF6::CoreAddons
    KF6::Svg
    KF6::I18n
    KF6::ConfigGui
    KF6::ConfigQml
)
