# CMake build for the Atelier3D desktop app (apps/desktop)
#   projects/atelier_engine   engine library
#   projects/atelier_editor   editor (AtelierEditor): create, pick and play game projects
#                             (games are created in projects/ next to the exe, see GameProject::ProjectsRoot)
#   projects/atelier_asset_tool  AtelierAssetTool: converts OBJ / FBX / glTF ... into the engine's .amodel (uses assimp)
#   ../../tests/unit          GoogleTest unit tests (top-level tests/ folder)
#   ../../tests/tool          GoogleTest tests of the asset tool
cmake_minimum_required (VERSION 3.20)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("Atelier3D" LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# compile_commands.json in the build directory, used by clang-tidy (tools/ci/lint.sh)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# All executables go to <build>/bin (<build>/bin/<config> with Visual Studio)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(ATELIER_PROJECTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/projects")
set(ATELIER_LIBRARIES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libraries")

get_property(ATELIER_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (ATELIER_IS_MULTI_CONFIG)
  set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Package" CACHE STRING "" FORCE)
endif ()

# Copy settings from Release to Package
set(CMAKE_C_FLAGS_PACKAGE     "${CMAKE_C_FLAGS_RELEASE}"     CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_PACKAGE   "${CMAKE_CXX_FLAGS_RELEASE}"   CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_PACKAGE "${CMAKE_EXE_LINKER_FLAGS_RELEASE}" CACHE STRING "" FORCE)

# Compiler warnings for our own targets (reported by CI)
function(atelier_enable_warnings target)
  if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # -Wno-missing-field-initializers: it fires on C++20 designated initializers such as
    # GameObject{.id = 1, .name = "Box"}, where the fields left out have default values, and on Win32's
    # WINDOWPLACEMENT{sizeof(WINDOWPLACEMENT)} pattern - both intended, so the warning is only noise here
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
  endif ()
endfunction()

# FMOD Core audio, vendored under libraries/ with one folder per platform:
#   libraries/include/FMOD/api/       headers (the same for every platform)
#   libraries/lib/FMOD/windows/       Windows libraries: <core|studio>/lib/<x64|x86>/  fmod.dll, fmod_vc.lib, ...
#   libraries/lib/FMOD/linux/         Linux libraries:   <core|studio>/lib/<x86_64|...>/  libfmod.so.14, ...
# MSVC and Linux GCC link FMOD's C++ API directly. MinGW cannot (the DLL's C++ names are MSVC-mangled), so for MinGW
# the engine adds src/audio/FmodMinGWBridge.cpp, which implements the C++ calls AudioSystem.cpp makes on top of FMOD's
# C API, and links fmod.dll directly. Other platforms compile the silent backend.
# Override with -DATELIER_ENABLE_FMOD=ON/OFF.
set(ATELIER_FMOD_INCLUDE_DIR "${ATELIER_LIBRARIES_DIR}/include/FMOD/api/core/inc")
set(ATELIER_FMOD_WINDOWS_DIR "${ATELIER_LIBRARIES_DIR}/lib/FMOD/windows")
set(ATELIER_FMOD_LINUX_DIR "${ATELIER_LIBRARIES_DIR}/lib/FMOD/linux")

if (WIN32)
  set(ATELIER_FMOD_LIB_DIR "${ATELIER_FMOD_WINDOWS_DIR}/core/lib")
  if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ATELIER_FMOD_ARCH "x64")
  else ()
    set(ATELIER_FMOD_ARCH "x86")
  endif ()
else ()
  set(ATELIER_FMOD_LIB_DIR "${ATELIER_FMOD_LINUX_DIR}/core/lib")
  set(ATELIER_FMOD_ARCH "x86_64")
endif ()

if ((MSVC OR MINGW) AND EXISTS "${ATELIER_FMOD_INCLUDE_DIR}/fmod.hpp")
  set(ATELIER_FMOD_DEFAULT ON)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SIZEOF_VOID_P EQUAL 8
        AND EXISTS "${ATELIER_FMOD_LIB_DIR}/${ATELIER_FMOD_ARCH}/libfmod.so.14")
  set(ATELIER_FMOD_DEFAULT ON)
else ()
  set(ATELIER_FMOD_DEFAULT OFF)
endif ()

option(ATELIER_ENABLE_FMOD "Build the FMOD audio backend (Windows, 64-bit Linux)" ${ATELIER_FMOD_DEFAULT})

if (ATELIER_ENABLE_FMOD)
  # fmodL is the logging build - much better errors, so Debug takes it.
  # ATELIER_FMOD_DLL is the runtime file copied next to the executables, ATELIER_FMOD_IMPLIB what the linker is given
  if (NOT WIN32)
    # Linked by its full name: libfmod.so.14 is also the name the executable looks for at runtime (its soname)
    set(ATELIER_FMOD_DLL "${ATELIER_FMOD_LIB_DIR}/${ATELIER_FMOD_ARCH}/$<IF:$<CONFIG:Debug>,libfmodL.so.14,libfmod.so.14>")
    set(ATELIER_FMOD_IMPLIB "${ATELIER_FMOD_DLL}")
  elseif (MINGW)
    # MinGW's linker reads a DLL's C exports directly, which is all the bridge needs
    set(ATELIER_FMOD_DLL "${ATELIER_FMOD_LIB_DIR}/${ATELIER_FMOD_ARCH}/$<IF:$<CONFIG:Debug>,fmodL.dll,fmod.dll>")
    set(ATELIER_FMOD_IMPLIB "${ATELIER_FMOD_DLL}")
  else ()
    set(ATELIER_FMOD_DLL "${ATELIER_FMOD_LIB_DIR}/${ATELIER_FMOD_ARCH}/$<IF:$<CONFIG:Debug>,fmodL.dll,fmod.dll>")
    set(ATELIER_FMOD_IMPLIB "${ATELIER_FMOD_LIB_DIR}/${ATELIER_FMOD_ARCH}/$<IF:$<CONFIG:Debug>,fmodL_vc.lib,fmod_vc.lib>")
  endif ()

  message(STATUS "FMOD audio backend: ENABLED (${ATELIER_FMOD_LIB_DIR}/${ATELIER_FMOD_ARCH})")
else ()
  message(STATUS "FMOD audio backend: disabled - the engine will run silently")
endif ()

# Linux executables look for shared libraries (libfmod.so.14) in their own folder first, so a build or package folder
# runs as it is, wherever it is copied
if (UNIX AND NOT APPLE)
  set(CMAKE_BUILD_RPATH "\$ORIGIN")
endif ()

# Without this the exe links fine and then won't start: "fmod.dll not found" / "libfmod.so.14: cannot open".
function(atelier_copy_fmod_dll target)
  if (NOT ATELIER_ENABLE_FMOD)
    return()
  endif ()
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ATELIER_FMOD_DLL}" "$<TARGET_FILE_DIR:${target}>"
    COMMENT "Copying FMOD runtime next to ${target}"
    VERBATIM
  )
endfunction()

# Linux links the system Lua (liblua5.4-dev). A copy goes next to the exe, under the name the exe asks for at runtime
# (liblua5.4.so.0), so packages and exported games start on a PC without it. Windows copies lua54.dll itself.
function(atelier_copy_lua_library target)
  get_filename_component(lua_file "${LUA_LIBRARY}" REALPATH)
  get_filename_component(lua_name "${lua_file}" NAME)
  if (NOT lua_name MATCHES "\\.so\\.[0-9]+")
    return() # a static Lua is already inside the exe
  endif ()
  string(REGEX REPLACE "(\\.so\\.[0-9]+).*$" "\\1" lua_soname "${lua_name}")
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${lua_file}" "$<TARGET_FILE_DIR:${target}>/${lua_soname}"
    COMMENT "Copying Lua runtime next to ${target}"
    VERBATIM
  )
endfunction()

# Off by default so normal builds only produce what the app needs; tools/ci/build.sh and build.bat turn it on
option(ATELIER_BUILD_TESTS "Build the unit tests in tests/unit at the repository root" OFF)

add_subdirectory("projects/atelier_engine")

add_subdirectory("projects/atelier_editor")

# What exported games run (the runtime without the editor); building the editor builds it beside it
add_subdirectory("projects/atelier_player")
add_dependencies(AtelierEditor AtelierPlayer)

# The asset tool the editor runs when a model is imported; it lands in <build>/bin next to the editor. assimp is vendored
# and takes a while to compile, so it can be switched off (the editor then imports .amodel files only).
option(ATELIER_BUILD_ASSET_TOOL "Build AtelierAssetTool (model converter, uses the vendored assimp)" ON)
if (ATELIER_BUILD_ASSET_TOOL)
  add_subdirectory("projects/atelier_asset_tool")
  add_dependencies(AtelierEditor AtelierAssetTool)
endif ()

# Unit tests run on the build machine, so skip them when cross-compiling
if (ATELIER_BUILD_TESTS AND NOT CMAKE_CROSSCOMPILING)
  enable_testing()
  add_subdirectory ("${CMAKE_CURRENT_SOURCE_DIR}/../../tests/unit" "${CMAKE_BINARY_DIR}/tests")
  if (ATELIER_BUILD_ASSET_TOOL)
    add_subdirectory ("${CMAKE_CURRENT_SOURCE_DIR}/../../tests/tool" "${CMAKE_BINARY_DIR}/tests_tool")
  endif ()
endif ()
