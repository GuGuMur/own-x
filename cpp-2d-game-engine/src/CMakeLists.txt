add_executable(cpp_2d_game_engine
        main.cpp
        glad.c
        stb.cpp
)

target_link_libraries(cpp_2d_game_engine PRIVATE SDL2::SDL2)

target_include_directories(cpp_2d_game_engine PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)