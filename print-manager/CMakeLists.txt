find_package(Qt6 REQUIRED COMPONENTS Core Quick Svg SerialPort Sql HttpServer Mqtt)
find_package(CURL REQUIRED)
find_package(libzip REQUIRED)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(appPolyhydranPrintManager
    src/main.cpp
)

qt_add_qml_module(appPolyhydranPrintManager
    URI PolyhydranPrintManager
    VERSION 1.0
    QML_FILES
        ui/Main.qml
        ui/RoundButtonC.qml
        ui/IconButtonC.qml
        ui/ScrollableLabel.qml
    RESOURCES
        resources/PC-LogoText.svg
        resources/ltx2a_ledoff.svg
        resources/ltx2a_ledon.svg
        resources/card_filled.svg
        resources/PC-Logo.ico
        resources/OrcaSlicer.svg
        resources/help.svg
        resources/upload_file.svg
        resources/progress_activity.svg
        resources/JetBrainsMono-Regular.ttf
    SOURCES
        include/bambuemulator.h
        src/bambuemulator.cpp
        include/bambulab.h
        src/bambulab.cpp
        include/errorhandler.hpp
        src/errorhandler.cpp
        include/errors.hpp
        include/ftpsclient.h
        src/ftpsclient.cpp
        include/gcodeparser.h
        src/gcodeparser.cpp
        include/ltx2aQT.h
        src/ltx2aQT.cpp
        include/octoprintemulator.h
        src/octoprintemulator.cpp
        include/printer.h
        src/printer.cpp
        include/printermanager.h
        src/printermanager.cpp
        include/prusa.h
        src/prusa.cpp
        include/qtbackend.h
        src/qtbackend.cpp
)

qt_import_qml_plugins(appPolyhydranPrintManager)

set_target_properties(appPolyhydranPrintManager PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PolyhydranPrintManager_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PolyhydranPrintManager_VERSION_MAJOR}.${PolyhydranPrintManager_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_include_directories(appPolyhydranPrintManager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(appPolyhydranPrintManager PRIVATE
    #PolyhydranPrintManagerShared
    Qt6::Core
    Qt6::Quick
    Qt6::Svg
    Qt6::SerialPort
    Qt6::Sql
    Qt6::HttpServer
    Qt6::Mqtt
    CURL::libcurl
    libzip::zip
)

include(GNUInstallDirs)
install(TARGETS appPolyhydranPrintManager
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)