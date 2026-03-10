find_package(Qt6 REQUIRED COMPONENTS Core Quick Svg Mqtt)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(appPolyhydranPrintManagerConfigTool
    src/main.cpp
)

qt_add_qml_module(appPolyhydranPrintManagerConfigTool
    URI PolyhydranPrintManagerConfigTool
    VERSION 1.0
    QML_FILES
        ui/Main.qml
    RESOURCES
        resources/PC-Logo.ico
        resources/SPolyDark.webm
        resources/SPolyWhite.webm
    SOURCES
        include/bambulab.h
        src/bambulab.cpp
        include/errorhandler.hpp
        src/errorhandler.cpp
        include/errors.hpp
        include/printer.h
        include/prusa.h
        src/prusa.cpp
        include/configmanager.h
        include/qtbackend.h
        QML_FILES ui/Startup.qml
)

qt_import_qml_plugins(appPolyhydranPrintManagerConfigTool)

set_target_properties(appPolyhydranPrintManagerConfigTool PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PolyhydranPrintManager_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PolyhydranPrintManager_VERSION_MAJOR}.${PolyhydranPrintManager_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_include_directories(appPolyhydranPrintManagerConfigTool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(appPolyhydranPrintManagerConfigTool PRIVATE
    #PolyhydranPrintManagerShared
    Qt6::Core
    Qt6::Quick
    Qt6::Svg
    Qt6::Mqtt
)

include(GNUInstallDirs)
install(TARGETS appPolyhydranPrintManagerConfigTool
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
