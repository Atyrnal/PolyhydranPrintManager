cmake_minimum_required(VERSION 3.21)
project(PolyhydranPrintManager VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_QML_GENERATE_QMLLS_INI ON)
 
# The config tool isn't ready yet, so it is skipped unless asked for:
#   cmake --preset arch-debug -DBUILD_CONFIG_TOOL=ON
option(BUILD_CONFIG_TOOL "Build the (work-in-progress) config tool" OFF)
 
#####
#add_subdirectory(shared)
add_subdirectory(print-manager)
if(BUILD_CONFIG_TOOL)
    add_subdirectory(config-tool)
endif()
 
