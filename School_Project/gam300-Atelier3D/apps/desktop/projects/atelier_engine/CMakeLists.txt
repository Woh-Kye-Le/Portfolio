cmake_minimum_required(VERSION 3.20)

# Engine library: public headers in include/engine/, sources in src/ (same folder layout)
file(GLOB_RECURSE ATELIER_ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

# Only compile the window backend for the current platform
if (WIN32)
    list(FILTER ATELIER_ENGINE_SOURCES EXCLUDE REGEX "/platform/GlfwWindow\\.(cpp|hpp)$")
else ()
    list(FILTER ATELIER_ENGINE_SOURCES EXCLUDE REGEX "/platform/WindowsWindow\\.(cpp|hpp)$")
endif ()

add_library(AtelierEngine STATIC ${ATELIER_ENGINE_SOURCES})

set(ATELIER_IMGUI_SOURCES
    "${ATELIER_LIBRARIES_DIR}/include/imgui/imgui.cpp"
    "${ATELIER_LIBRARIES_DIR}/include/imgui/imgui_draw.cpp"
    "${ATELIER_LIBRARIES_DIR}/include/imgui/imgui_tables.cpp"
    "${ATELIER_LIBRARIES_DIR}/include/imgui/imgui_widgets.cpp"
    # Move / rotate / scale gizmos for the editor viewport
    "${ATELIER_LIBRARIES_DIR}/include/ImGuizmo/ImGuizmo.cpp"
)

target_sources(AtelierEngine PRIVATE ${ATELIER_IMGUI_SOURCES})
# ImGuizmo includes "imgui.h" / "imconfig.h" without the imgui/ folder
target_include_directories(AtelierEngine PUBLIC "${ATELIER_LIBRARIES_DIR}/include/imgui")

# Asset loading: OpenFBX (.fbx) with its bundled libdeflate, stb_vorbis (.ogg)
set(ATELIER_ASSET_LIBRARY_SOURCES
    "${ATELIER_LIBRARIES_DIR}/include/stb/stb_vorbis.c"
    "${ATELIER_LIBRARIES_DIR}/include/OpenFBX/ofbx.cpp"
    "${ATELIER_LIBRARIES_DIR}/include/OpenFBX/libdeflate.c"
)
target_sources(AtelierEngine PRIVATE ${ATELIER_ASSET_LIBRARY_SOURCES})

target_include_directories(AtelierEngine
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${ATELIER_LIBRARIES_DIR}/include"
        # nlohmann/json (header-only), used by engine/serializer/JsonSerializer.hpp
        "${ATELIER_LIBRARIES_DIR}/include/json/include"
)

# Root of apps/desktop so Renderer can resolve shader paths via GAM300_ROOT_DIR
target_compile_definitions(AtelierEngine PUBLIC
    "GAM300_ROOT_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/../..\""
)

if (WIN32)
    target_compile_definitions(AtelierEngine PUBLIC
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    )

    # shell32: files dropped on the window; dwmapi: frames paced by the desktop compositor (WindowsWindow::Present)
    target_link_libraries(AtelierEngine PUBLIC opengl32 gdi32 user32 shell32 dwmapi)

    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ATELIER_ARCH "x64")
    else ()
        set(ATELIER_ARCH "x86")
    endif ()

    # GLEW
    target_link_directories(AtelierEngine PUBLIC
        "${ATELIER_LIBRARIES_DIR}/lib/GLEW/${ATELIER_ARCH}"
    )
    target_link_libraries(AtelierEngine PUBLIC
        optimized glew32
        debug glew32d
    )

    # GLFW
    target_link_directories(AtelierEngine PUBLIC
        "${ATELIER_LIBRARIES_DIR}/lib/GLFW/${ATELIER_ARCH}"
    )
    target_link_libraries(AtelierEngine PUBLIC
        optimized glfw3
        debug glfw3d
    )

    # FMOD Core
    target_link_directories(AtelierEngine PUBLIC
        "${ATELIER_FMOD_WINDOWS_DIR}/core/lib/${ATELIER_ARCH}"
    )
    target_link_libraries(AtelierEngine PUBLIC
        optimized fmod_vc
        debug fmodL_vc
    )

    # FMOD Studio
    target_link_directories(AtelierEngine PUBLIC
        "${ATELIER_FMOD_WINDOWS_DIR}/studio/lib/${ATELIER_ARCH}"
    )
    target_link_libraries(AtelierEngine PUBLIC
        optimized fmodstudio_vc
        debug fmodstudioL_vc
    )

    # LUA
    target_link_directories(AtelierEngine PUBLIC
        "${ATELIER_LIBRARIES_DIR}/lib/LUA"
    )
    target_link_libraries(AtelierEngine PUBLIC lua54)
else ()
    find_package(OpenGL REQUIRED)

    # GLFW 3.4 is built from source to match the bundled 3.4 headers
    # (Ubuntu's libglfw3-dev is 3.3, which would fail to link)
    include(FetchContent)
    # Use extraction time as file timestamps for downloaded archives
    if (POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif ()
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glfw
        URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
        URL_HASH SHA256=b5ec004b2712fd08e8861dc271428f048775200a2df719ccf575143ba749a3e9
    )
    FetchContent_MakeAvailable(glfw)

    target_sources(AtelierEngine PRIVATE "${ATELIER_LIBRARIES_DIR}/include/imgui/backends/imgui_impl_glfw.cpp")
    # imgui backends include "imgui.h" without the imgui/ prefix
    target_include_directories(AtelierEngine PRIVATE "${ATELIER_LIBRARIES_DIR}/include/imgui")
    target_link_libraries(AtelierEngine PUBLIC OpenGL::GL glfw ${CMAKE_DL_LIBS})

    # GLEW (OpenGL function loader): libraries/ only has Windows builds of it, so on Linux its source is compiled into
    # the engine. The release's own header is put before libraries/include (whose glew.h is an older version), so the
    # header always matches glew.c. GLEW_NO_GLU: nothing uses GLU, so GL/glu.h is not needed on the build machine.
    # SOURCE_SUBDIR names a folder with no CMakeLists.txt, so only the source is downloaded (GLEW's own build is skipped).
    FetchContent_Declare(glew
        URL https://github.com/nigels-com/glew/releases/download/glew-2.3.1/glew-2.3.1.tgz
        URL_HASH SHA256=b64790f94b926acd7e8f84c5d6000a86cb43967bd1e688b03089079799c9e889
        SOURCE_SUBDIR source-only
    )
    FetchContent_MakeAvailable(glew)
    target_sources(AtelierEngine PRIVATE "${glew_SOURCE_DIR}/src/glew.c")
    target_include_directories(AtelierEngine BEFORE PUBLIC "${glew_SOURCE_DIR}/include")
    target_compile_definitions(AtelierEngine PUBLIC GLEW_STATIC GLEW_NO_GLU)
    if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        set_source_files_properties("${glew_SOURCE_DIR}/src/glew.c" PROPERTIES COMPILE_OPTIONS "-w")
    endif ()

    # imgui_impl_glfw calls Xlib directly on Linux/BSD
    if (NOT APPLE)
        find_package(X11 REQUIRED)
        target_link_libraries(AtelierEngine PUBLIC X11::X11)
    endif ()
endif ()

# PRIVATE include/define: AudioSystem.hpp is pimpl'd, so FMOD never leaks out
# to anything that links the engine.
if (ATELIER_ENABLE_FMOD)
    target_include_directories(AtelierEngine PRIVATE "${ATELIER_FMOD_INCLUDE_DIR}")
    target_link_libraries(AtelierEngine PUBLIC "${ATELIER_FMOD_IMPLIB}")
    target_compile_definitions(AtelierEngine PRIVATE ATELIER_HAS_FMOD)
endif ()

# Jolt Physics (rigid bodies, collisions, raycasts), built from source with the engine, so the same version runs with
# MSVC, Linux GCC and the MinGW cross-build. On the course's approved library list ("Recommend JoltPhysics").
# Only the Jolt library itself is built: its tests, samples and viewer are skipped when it is not the top-level project.
# Offline: download the zip once and pass -DFETCHCONTENT_SOURCE_DIR_JOLTPHYSICS=<its unzipped folder>.
include(FetchContent)
if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif ()
# The same C runtime as the rest of the project. Jolt defaults to the static one, which fails to link with ours (LNK2038)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
# Jolt's own warnings-as-errors and link-time optimisation would break builds with newer compilers or MinGW
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
# GPU compute (not used) would need the DirectX / Vulkan shader compilers on every machine
set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
set(JPH_USE_CPU_COMPUTE OFF CACHE BOOL "" FORCE)
# CPU instruction sets: SSE4.2 runs on any 64-bit PC from the last 15 years. AVX and later are off: a lab or CI machine
# without them would crash on start, and MinGW's GCC cannot align the stack for AVX on Windows
foreach (ATELIER_JOLT_ISA USE_AVX USE_AVX2 USE_AVX512 USE_LZCNT USE_TZCNT USE_F16C USE_FMADD)
    set(${ATELIER_JOLT_ISA} OFF CACHE BOOL "" FORCE)
endforeach ()
FetchContent_Declare(JoltPhysics
    URL https://github.com/jrouwe/JoltPhysics/archive/refs/tags/v5.6.0.zip
    URL_HASH SHA256=0af9beea51637ef805e624fe838ea2870f7b68cd48cbe1615c853bd9bcf4f1d7
    SOURCE_SUBDIR Build
)
FetchContent_MakeAvailable(JoltPhysics)

# Jolt's headers count as system headers, so our -Wall -Wextra (and clang-tidy) report nothing inside them
get_target_property(ATELIER_JOLT_INCLUDES Jolt INTERFACE_INCLUDE_DIRECTORIES)
set_target_properties(Jolt PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${ATELIER_JOLT_INCLUDES}")
# Jolt's own sources: no warnings in the build log, so the CI warnings report only shows our code
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(Jolt PRIVATE -w)
endif ()

# PRIVATE: PhysicsWorld.hpp is pimpl'd, so Jolt's headers and settings never reach the editor or the tests
target_link_libraries(AtelierEngine PRIVATE Jolt::Jolt)

atelier_enable_warnings(AtelierEngine)

# Third-party sources compiled into the engine: silence their warnings so reports only show our code
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(
        ${ATELIER_IMGUI_SOURCES}
        ${ATELIER_ASSET_LIBRARY_SOURCES}
        "${ATELIER_LIBRARIES_DIR}/include/imgui/backends/imgui_impl_glfw.cpp"
        PROPERTIES COMPILE_OPTIONS "-w"
    )
endif ()

# Windows already links lua54 earlier in this file.
if (WIN32)
    find_path(ATELIER_LUA_INCLUDE_DIR lua.h
        PATHS "${ATELIER_LIBRARIES_DIR}/include"
        PATH_SUFFIXES
            LUA Lua lua lua54 lua5.4
            LUA/include Lua/include lua/include
        REQUIRED
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH # the MinGW toolchain would otherwise look only under its own /usr folder
    )

    target_include_directories(AtelierEngine PRIVATE
        "${ATELIER_LUA_INCLUDE_DIR}"
    )
else ()
    find_package(Lua 5.4 EXACT REQUIRED)

    target_include_directories(AtelierEngine PRIVATE
        ${LUA_INCLUDE_DIR}
    )

    target_link_libraries(AtelierEngine PUBLIC
        ${LUA_LIBRARIES}
    )
endif ()
