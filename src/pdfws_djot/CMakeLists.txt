cmake_minimum_required(VERSION 3.16)

add_library(pdfws_djot STATIC
    ProvenanceGuard.h       ProvenanceGuard.cpp
    IDjotCodec.h
    LuaDjotCodec.h          LuaDjotCodec.cpp
    IDjotMapper.h
    PdfStructureMapper.h    PdfStructureMapper.cpp
)

target_include_directories(pdfws_djot PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(pdfws_djot PUBLIC
    docmodel
    liblua
)

# Suppress warnings from vendored Lua headers in our TUs
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pdfws_djot PRIVATE -Wno-old-style-cast)
endif()
