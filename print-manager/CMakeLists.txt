find_package(Qt6 REQUIRED COMPONENTS Core Quick Svg SerialPort Sql HttpServer Mqtt)
find_package(CURL REQUIRED)
find_package(libzip REQUIRED)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(appPolyhydranPrintManager
    src/main.cpp
)
file(GLOB_RECURSE QML_SOURCE_FILES   CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "ui/*.qml")
file(GLOB_RECURSE RESOURCE_FILES     CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "resources/*")
file(GLOB_RECURSE HEADER_FILES       CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "include/*.h" "include/*.hpp")
file(GLOB_RECURSE CPP_SOURCE_FILES   CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "src/*.cpp")
set_source_files_properties(ui/Theme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)
qt_add_qml_module(appPolyhydranPrintManager
    URI PolyhydranPrintManager
    VERSION 1.0
    QML_FILES  ${QML_SOURCE_FILES}
    RESOURCES  ${RESOURCE_FILES}
    SOURCES    ${HEADER_FILES} ${CPP_SOURCE_FILES}
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