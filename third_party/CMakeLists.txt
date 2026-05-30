cmake_minimum_required(VERSION 3.16)

# Lua is a C library — enable C compilation for this subdirectory
enable_language(C)

# ── Lua 5.4 (MIT License) ─────────────────────────────────────────────────
file(GLOB LUA_SRC "${CMAKE_CURRENT_SOURCE_DIR}/lua-5.4/src/*.c")
# Remove the standalone interpreter and compiler mains
list(REMOVE_ITEM LUA_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/lua-5.4/src/lua.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lua-5.4/src/luac.c"
)

add_library(liblua STATIC ${LUA_SRC})
target_include_directories(liblua PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lua-5.4/src)

# Suppress warnings in vendored code
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(liblua PRIVATE -w)
endif()
