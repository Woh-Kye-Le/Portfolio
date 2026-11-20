cmake_minimum_required(VERSION 3.20)

# Tests of the asset tool (apps/desktop/projects/atelier_asset_tool): converting OBJ / glTF / FBX into .amodel.
# Its own executable, because the converter links assimp and the engine must never see it; googletest comes from
# tests/unit, which is added first.
file(GLOB ATELIER_TOOL_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_executable(AtelierToolTests ${ATELIER_TOOL_TEST_SOURCES})
target_link_libraries(AtelierToolTests PRIVATE AtelierAssetConverter GTest::gtest_main)
atelier_enable_warnings(AtelierToolTests)

include(GoogleTest)
gtest_discover_tests(AtelierToolTests)
