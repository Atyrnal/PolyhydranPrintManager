find_package(Qt6 REQUIRED COMPONENTS Core Quick Svg)

qt_add_library(PolyhydranPrintManagerShared)

# target_include_directories(PolyhydranPrintManagerShared PUBLIC
#     ${CMAKE_CURRENT_SOURCE_DIR}/include
# )

target_link_libraries(PolyhydranPrintManagerShared PUBLIC
    Qt6::Core
    Qt6::Quick
    Qt6::Svg
)

target_compile_features(PolyhydranPrintManagerShared PUBLIC cxx_std_17)