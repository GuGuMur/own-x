cmake_minimum_required(VERSION 3.15)
project(cpp_2d_game_engine VERSION 1.0.0 LANGUAGES C CXX)

add_subdirectory(src)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


include(FetchContent)

FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.28.5
        GIT_SHALLOW TRUE
)
set(SDL_TEST OFF CACHE BOOL "Disable SDL tests")
set(SDL_SHARED ON CACHE BOOL "Build shared library")
FetchContent_MakeAvailable(SDL2)
target_link_libraries(cpp_2d_game_engine PRIVATE SDL2::SDL2)


set(LUA_LIBRARY "/lib/x86_64-linux-gnu/liblua5.4.so")
set(LUA_INCLUDE_DIR "/usr/include/lua5.4")

target_include_directories(cpp_2d_game_engine PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        /usr/include
)

add_definitions(-DSTB_VORBIS_HEADER_ONLY)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/data)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
