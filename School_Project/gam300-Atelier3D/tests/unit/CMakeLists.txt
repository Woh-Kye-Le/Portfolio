cmake_minimum_required(VERSION 3.20)

include(FetchContent)
# Use extraction time as file timestamps for downloaded archives
if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif ()

# Match the MSVC runtime used by the rest of the project
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(googletest
    URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
    URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
)
FetchContent_MakeAvailable(googletest)

file(GLOB ATELIER_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_executable(AtelierTests ${ATELIER_TEST_SOURCES})

target_link_libraries(AtelierTests PRIVATE AtelierEngine GTest::gtest_main)

# Sample asset files used by the tests (tests/unit/data)
target_compile_definitions(AtelierTests PRIVATE ATELIER_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
# The sample game committed in the repo (projects/atelier_editor: configs/, data/scenes/, assets/)
target_compile_definitions(AtelierTests PRIVATE ATELIER_SAMPLE_GAME_DIR="${ATELIER_PROJECTS_DIR}/atelier_editor")

# The asset tool the import tests run (they skip themselves when the tool was not built)
if (TARGET AtelierAssetTool)
    add_dependencies(AtelierTests AtelierAssetTool)
    target_compile_definitions(AtelierTests PRIVATE ATELIER_ASSET_TOOL_PATH="$<TARGET_FILE:AtelierAssetTool>")
endif ()

# Editor classes under test (header-only)
target_include_directories(AtelierTests PRIVATE "${ATELIER_PROJECTS_DIR}/atelier_editor/include")

atelier_enable_warnings(AtelierTests)

# Linux: FMOD's library next to the test binary (it is run right after linking, to list the tests)
if (NOT WIN32)
    atelier_copy_fmod_dll(AtelierTests)
endif ()

# Registers every TEST() with CTest so each one shows up individually in reports
include(GoogleTest)
gtest_discover_tests(AtelierTests)
