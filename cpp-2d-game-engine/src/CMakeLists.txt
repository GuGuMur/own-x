add_executable(cpp_2d_game_engine
        main.cpp
        glad.c
        stb.cpp
        miniz.c
)


target_link_libraries(cpp_2d_game_engine PRIVATE
        SDL2::SDL2
        lua5.4
        GL
        m
)

target_include_directories(cpp_2d_game_engine PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${LUA_INCLUDE_DIR}
)

# 把 Lua 脚本文件复制到构建目录
#configure_file(${CMAKE_CURRENT_SOURCE_DIR}/main.lua ${CMAKE_CURRENT_BINARY_DIR}/main.lua COPYONLY)

file(GLOB LUA_FILES_IN_SRC "${CMAKE_CURRENT_SOURCE_DIR}/*.lua")
foreach(lua_file ${LUA_FILES_IN_SRC})
    get_filename_component(filename ${lua_file} NAME)
    configure_file(
            ${lua_file}
            ${CMAKE_CURRENT_BINARY_DIR}/${filename}
            COPYONLY
    )
endforeach()