cmake_minimum_required(VERSION 3.16)

project(PCMakerspace3DPKiosk VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optional: force the linker to look in vcpkg
# link_directories("C:/vcpkg/installed/x64-windows/lib")
# include_directories("C:/vcpkg/installed/x64-windows/include")

#######

# link_directories("C:/Libs/Curl/lib")


# set(CURL_INCLUDE_DIR "C:/Libs/Curl/include")
# set(CURL_LIBRARY "C:/Libs/Curl/lib/libcurl.a")

# set(ZIP_INCLUDE_DIR "C:/msys64/mingw64/include")
# set(ZIP_LIBRARY "C:/msys64/mingw64/lib/libzip.dll.a")

find_package(Qt6 REQUIRED COMPONENTS Core Quick SerialPort Svg Sql HttpServer Mqtt)
find_package(CURL REQUIRED)
#find_package(ZIP REQUIRED)
find_package(libzip REQUIRED)

# include_directories("${CURL_INCLUDE_DIR}")
# include_directories("${ZIP_INCLUDE_DIR}")


qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(appPCMakerspace3DPKiosk
    src/main.cpp
)

qt_add_qml_module(appPCMakerspace3DPKiosk
    URI PCMakerspace3DPKiosk
    VERSION 1.0
    QML_FILES
        ui/Main.qml
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
        headers/ltx2aQT.h
        src/ltx2aQT.cpp

        headers/qtbackend.h
        src/qtbackend.cpp
        headers/gcodeparser.h
        src/gcodeparser.cpp


        headers/octoprintemulator.h
        src/octoprintemulator.cpp
        SOURCES src/prusa.cpp
        SOURCES headers/prusa.h
        SOURCES headers/printer.h
        SOURCES src/printer.cpp
        SOURCES headers/bambulab.h
        SOURCES src/bambulab.cpp
        SOURCES headers/ftpsclient.h
        SOURCES src/ftpsclient.cpp
        SOURCES headers/printermanager.h
        SOURCES src/printermanager.cpp
        SOURCES headers/bambuemulator.h
        SOURCES src/bambuemulator.cpp
        QML_FILES ui/RoundButtonC.qml
        QML_FILES ui/IconButtonC.qml
        QML_FILES ui/ScrollableLabel.qml
        SOURCES headers/errors.hpp
        SOURCES src/errorhandler.cpp
        SOURCES headers/errorhandler.hpp
)

qt_import_qml_plugins(appPCMakerspace3DPKiosk)


# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appPCMakerspace3DPKiosk PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appPCMakerspace3DPKiosk
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appPCMakerspace3DPKiosk
    PRIVATE Qt6::Quick Qt6::SerialPort Qt6::Svg Qt6::Sql Qt6::HttpServer Qt6::Mqtt #libcurl
)
target_link_libraries(appPCMakerspace3DPKiosk PRIVATE Qt6::Core)
target_link_libraries(appPCMakerspace3DPKiosk PRIVATE CURL::libcurl libzip::zip)
# target_link_libraries(appPCMakerspace3DPKiosk PRIVATE ${CURL_LIBRARY})
# target_link_libraries(appPCMakerspace3DPKiosk PRIVATE ${ZIP_LIBRARY})

include(GNUInstallDirs)
install(TARGETS appPCMakerspace3DPKiosk
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
