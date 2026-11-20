# Atelier asset tool: converts model files (OBJ, FBX, glTF, ...) into the engine's own .amodel format.
#
#   AtelierAssetConverter   static library: the conversion itself (assimp + the engine's .amodel writer)
#   AtelierAssetTool        command line: AtelierAssetTool <input> <output> [--textures-dir <dir>] [--scale <f>] ...
#                           (input: a model file, or a folder or .zip with one; output: a .amodel file or a folder)
#
# assimp is vendored in libraries/include/assimp and is linked here and ONLY here: the engine, the editor and the games
# never see it (the course library list allows assimp for tools, not for the engine). The editor runs AtelierAssetTool
# from the folder it sits in, so it is built next to the editor in <build>/bin.
# 3.22: what assimp's own CMakeLists.txt asks for
cmake_minimum_required(VERSION 3.22)

# assimp: a static library with only the importers we ship, no exporters, tools, tests or installs.
# These are cache variables because assimp declares them with option(), and they must win over its defaults.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_DRACO OFF CACHE BOOL "" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
foreach (format OBJ FBX GLTF COLLADA 3DS PLY STL)
    set(ASSIMP_BUILD_${format}_IMPORTER ON CACHE BOOL "" FORCE)
endforeach ()
# assimp adds /Zi itself, which clashes with the /ZI (Edit and Continue) the top-level CMake picks for Debug: every file
# would print "overriding /Zi with /ZI". The tool has nothing to hot reload, so it takes plain /Zi (this directory only).
if (MSVC AND POLICY CMP0141)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>")
endif ()
# assimp's DLL copying step (a target nothing builds here) calls add_custom_command the old way, and CMake 3.31+ prints
# a warning for each call. The old behaviour is fine for it, and the vendored files stay as they were released.
set(CMAKE_POLICY_DEFAULT_CMP0175 OLD)
add_subdirectory("${ATELIER_LIBRARIES_DIR}/include/assimp" "${CMAKE_CURRENT_BINARY_DIR}/assimp" EXCLUDE_FROM_ALL)

add_library(AtelierAssetConverter STATIC
    src/AssetConverter.cpp
    src/ZipReader.cpp
    # The one engine file the tool needs: the .amodel writer/reader (no other engine code, so no OpenGL or window)
    "${ATELIER_PROJECTS_DIR}/atelier_engine/src/assets/loaders/AtelierModel.cpp"
)
target_include_directories(AtelierAssetConverter PUBLIC
    include
    "${ATELIER_PROJECTS_DIR}/atelier_engine/include"
    "${ATELIER_LIBRARIES_DIR}/include"
)
target_link_libraries(AtelierAssetConverter PUBLIC assimp::assimp)
atelier_enable_warnings(AtelierAssetConverter)

add_executable(AtelierAssetTool src/main.cpp)
target_link_libraries(AtelierAssetTool PRIVATE AtelierAssetConverter)
atelier_enable_warnings(AtelierAssetTool)
