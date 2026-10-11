cmake_minimum_required(VERSION 3.20)

project(Oceanbound LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ALPHA_ENGINE_DIR "${CMAKE_SOURCE_DIR}/Extern/AlphaEngine")

file(GLOB OCEANBOUND_SOURCES
    "${CMAKE_SOURCE_DIR}/Oceanbound/*.cpp"
)

add_executable(Oceanbound WIN32 ${OCEANBOUND_SOURCES})

target_include_directories(Oceanbound PRIVATE
    "${CMAKE_SOURCE_DIR}/Oceanbound"
    "${ALPHA_ENGINE_DIR}/include"
)

target_link_directories(Oceanbound PRIVATE "${ALPHA_ENGINE_DIR}/lib")

target_link_libraries(Oceanbound PRIVATE
    debug Alpha_EngineD
    optimized Alpha_Engine
)

add_custom_command(TARGET Oceanbound POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Oceanbound/Assets"
        "$<TARGET_FILE_DIR:Oceanbound>/Assets"
)

add_custom_command(TARGET Oceanbound POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ALPHA_ENGINE_DIR}/lib/fmod.dll"
        "${ALPHA_ENGINE_DIR}/lib/fmodL.dll"
        "${ALPHA_ENGINE_DIR}/lib/freetype.dll"
        "$<TARGET_FILE_DIR:Oceanbound>/"
)

add_custom_command(TARGET Oceanbound POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<$<CONFIG:Debug>:${ALPHA_ENGINE_DIR}/lib/Alpha_EngineD.dll>"
        "$<$<NOT:$<CONFIG:Debug>>:${ALPHA_ENGINE_DIR}/lib/Alpha_Engine.dll>"
        "$<TARGET_FILE_DIR:Oceanbound>/"
)
