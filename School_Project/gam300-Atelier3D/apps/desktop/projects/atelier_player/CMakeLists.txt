cmake_minimum_required(VERSION 3.20)

# AtelierPlayer: what an exported game runs. Only the game runtime (the window, the game and its HUD), none of the
# editor; BuildGame copies it (renamed after the game) next to the game's data. Export from a Release or Package build
# so the game runs at full speed and needs no debug runtime on other computers.
set(ATELIER_EDITOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../atelier_editor")

add_executable(AtelierPlayer
    "${CMAKE_CURRENT_SOURCE_DIR}/src/PlayerMain.cpp"
    # The runtime app and the HUD it draws, with the editor's fonts and colours (Theme), shared with the editor's
    # --play
    "${ATELIER_EDITOR_DIR}/src/runtime/RuntimeApp.cpp"
    "${ATELIER_EDITOR_DIR}/src/runtime/GameHud.cpp"
    "${ATELIER_EDITOR_DIR}/src/Theme.cpp"
)

target_include_directories(AtelierPlayer PRIVATE "${ATELIER_EDITOR_DIR}/include")
target_link_libraries(AtelierPlayer PRIVATE AtelierEngine)
atelier_enable_warnings(AtelierPlayer)

# A game opens its window without a console behind it (its log is written to a file); main() stays the entry point
if (WIN32)
    set_target_properties(AtelierPlayer PROPERTIES WIN32_EXECUTABLE ON)
    if (MSVC)
        target_link_options(AtelierPlayer PRIVATE "/ENTRY:mainCRTStartup")
    endif ()
endif ()

# The libraries it runs with (the build's own flavour of each), and the HUD's fonts, next to it
if (WIN32)
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ATELIER_PLAYER_ARCH "x64")
    else ()
        set(ATELIER_PLAYER_ARCH "x86")
    endif ()
    add_custom_command(TARGET AtelierPlayer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ATELIER_LIBRARIES_DIR}/bin/GLEW/${ATELIER_PLAYER_ARCH}/$<IF:$<CONFIG:Debug>,glew32d.dll,glew32.dll>"
            "${ATELIER_FMOD_WINDOWS_DIR}/core/lib/${ATELIER_PLAYER_ARCH}/$<IF:$<CONFIG:Debug>,fmodL.dll,fmod.dll>"
            "${ATELIER_FMOD_WINDOWS_DIR}/studio/lib/${ATELIER_PLAYER_ARCH}/$<IF:$<CONFIG:Debug>,fmodstudioL.dll,fmodstudio.dll>"
            "${ATELIER_LIBRARIES_DIR}/bin/lua/lua54.dll"
            "$<TARGET_FILE_DIR:AtelierPlayer>"
        COMMENT "AtelierPlayer: copying its DLLs (${ATELIER_PLAYER_ARCH})"
        VERBATIM
    )
else ()
    atelier_copy_fmod_dll(AtelierPlayer)
    atelier_copy_lua_library(AtelierPlayer)
endif ()
add_custom_command(TARGET AtelierPlayer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${ATELIER_EDITOR_DIR}/fonts" "$<TARGET_FILE_DIR:AtelierPlayer>/assets/fonts"
    COMMENT "AtelierPlayer: copying the HUD fonts"
    VERBATIM
)
