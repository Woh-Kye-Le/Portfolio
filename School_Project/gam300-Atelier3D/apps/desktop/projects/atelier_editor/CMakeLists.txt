cmake_minimum_required(VERSION 3.20)

file(GLOB_RECURSE ATELIER_EDITOR_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

add_executable(AtelierEditor ${ATELIER_EDITOR_SOURCES})

target_include_directories(AtelierEditor
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(AtelierEditor PRIVATE AtelierEngine)

# Dev builds read assets straight from the source tree, so editing a config and
# pressing reload needs no rebuild. Packaged builds fall back to the copy next
# to the exe, which the POST_BUILD step below provides.
target_compile_definitions(AtelierEditor
    PRIVATE
        ATELIER_EDITOR_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

atelier_enable_warnings(AtelierEditor)

# Post-build: copy required DLLs next to the editor executable
if (WIN32)
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ATELIER_ARCH "x64")
    else ()
        set(ATELIER_ARCH "x86")
    endif ()

    # GLEW DLLs (Release + Debug so either config runs)
    add_custom_command(TARGET AtelierEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ATELIER_LIBRARIES_DIR}/bin/GLEW/${ATELIER_ARCH}/glew32.dll"
            "$<TARGET_FILE_DIR:AtelierEditor>/glew32.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ATELIER_LIBRARIES_DIR}/bin/GLEW/${ATELIER_ARCH}/glew32d.dll"
            "$<TARGET_FILE_DIR:AtelierEditor>/glew32d.dll"
        COMMENT "AtelierEditor: copying GLEW DLLs (${ATELIER_ARCH})"
    )

    # FMOD Core DLLs
    add_custom_command(TARGET AtelierEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ATELIER_FMOD_WINDOWS_DIR}/core/lib/${ATELIER_ARCH}/fmod.dll"
            "$<TARGET_FILE_DIR:AtelierEditor>/fmod.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ATELIER_FMOD_WINDOWS_DIR}/core/lib/${ATELIER_ARCH}/fmodL.dll"
            "$<TARGET_FILE_DIR:AtelierEditor>/fmodL.dll"
        COMMENT "AtelierEditor: copying FMOD Core DLLs (${ATELIER_ARCH})"
    )

    # FMOD Studio DLLs
    add_custom_command(TARGET AtelierEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ATELIER_FMOD_WINDOWS_DIR}/studio/lib/${ATELIER_ARCH}/fmodstudio.dll"
            "$<TARGET_FILE_DIR:AtelierEditor>/fmodstudio.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ATELIER_FMOD_WINDOWS_DIR}/studio/lib/${ATELIER_ARCH}/fmodstudioL.dll"
            "$<TARGET_FILE_DIR:AtelierEditor>/fmodstudioL.dll"
        COMMENT "AtelierEditor: copying FMOD Studio DLLs (${ATELIER_ARCH})"
    )

    # LUA DLL
    add_custom_command(TARGET AtelierEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ATELIER_LIBRARIES_DIR}/bin/lua/lua54.dll"
            "$<TARGET_FILE_DIR:AtelierEditor>/lua54.dll"
        COMMENT "AtelierEditor: copying LUA DLL"
    )
endif ()

# Linux: FMOD's library (libfmod.so.14, from libraries/lib/FMOD/linux/) next to the editor, where it is found at
# runtime; packages and built games copy it from here. (Windows copies fmod.dll in the block above.)
if (NOT WIN32)
    atelier_copy_fmod_dll(AtelierEditor)
    atelier_copy_lua_library(AtelierEditor)
endif ()

# Editor assets sit next to the exe, same rule as GameProject::ProjectsRoot().
# Builtin shaders (Standard3D, Unlit, FallbackUnlit, Sky) plus the
# BlinnPhong include are copied onto <ExeDir>/assets/shaders/Builtin/ so the
# Renderer's ResolveBuiltinShader exe-side fallback finds them when the
# engine source tree isn't reachable from the binary.
set(ATELIER_BUILTIN_SHADERS_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/../atelier_engine/src/assets/Shaders/Builtin")
set(ATELIER_EDITOR_BUILTIN_SHADERS_DST
    "$<TARGET_FILE_DIR:AtelierEditor>/assets/shaders/Builtin")
add_custom_command(TARGET AtelierEditor POST_BUILD
    # Emptied first: copy_directory only adds, so files removed from the source would otherwise stay behind
    COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:AtelierEditor>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:AtelierEditor>/assets"
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${ATELIER_EDITOR_BUILTIN_SHADERS_DST}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ATELIER_BUILTIN_SHADERS_SRC}/Standard3D.vert"
            "${ATELIER_BUILTIN_SHADERS_SRC}/Standard3D.frag"
            "${ATELIER_BUILTIN_SHADERS_SRC}/Unlit.vert"
            "${ATELIER_BUILTIN_SHADERS_SRC}/Unlit.frag"
            "${ATELIER_BUILTIN_SHADERS_SRC}/FallbackUnlit.vert"
            "${ATELIER_BUILTIN_SHADERS_SRC}/FallbackUnlit.frag"
            "${ATELIER_BUILTIN_SHADERS_SRC}/Sky.vert"
            "${ATELIER_BUILTIN_SHADERS_SRC}/Sky.frag"
            "${ATELIER_EDITOR_BUILTIN_SHADERS_DST}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${ATELIER_BUILTIN_SHADERS_SRC}/Common"
            "${ATELIER_EDITOR_BUILTIN_SHADERS_DST}/Common"
    # The editor's UI fonts (OFL, licences beside them). Kept out of assets/ above, which also seeds new games.
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/fonts" "$<TARGET_FILE_DIR:AtelierEditor>/assets/fonts"
    # The editor's own sounds (clicks, blocks snapping), kept out of assets/ for the same reason
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/sounds" "$<TARGET_FILE_DIR:AtelierEditor>/assets/sounds"
    COMMENT "Copying editor assets and builtin shaders next to AtelierEditor"
    VERBATIM
)

# The demo scenes are templates for new games, not a game of their own: the sample game (its configs, demo scenes
# and assets) is copied to <ExeDir>/templates/demos/, where GameProject::TemplatesDir() finds it. New games start
# empty or from one of these scenes, so the editor starts with no game until one is created.
set(ATELIER_TEMPLATES_DIR "$<TARGET_FILE_DIR:AtelierEditor>/templates/demos")
add_custom_command(TARGET AtelierEditor POST_BUILD
    # Emptied first, as above: a scene or asset removed from the sample game goes from the templates too
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${ATELIER_TEMPLATES_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ATELIER_TEMPLATES_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/configs" "${ATELIER_TEMPLATES_DIR}/configs"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/data" "${ATELIER_TEMPLATES_DIR}/data"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets" "${ATELIER_TEMPLATES_DIR}/assets"
    COMMENT "AtelierEditor: copying the demo templates to templates/demos/"
    VERBATIM
)
