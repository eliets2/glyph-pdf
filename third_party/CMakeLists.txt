cmake_minimum_required(VERSION 3.16)

file(GLOB LUA_SRC "lua-5.4/src/*.c")
list(REMOVE_ITEM LUA_SRC "${CMAKE_CURRENT_SOURCE_DIR}/lua-5.4/src/lua.c" "${CMAKE_CURRENT_SOURCE_DIR}/lua-5.4/src/luac.c")

add_library(liblua STATIC ${LUA_SRC})
target_include_directories(liblua PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lua-5.4/src)
