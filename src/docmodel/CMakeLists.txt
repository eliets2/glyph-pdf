cmake_minimum_required(VERSION 3.20)
project(docmodel LANGUAGES CXX)



add_library(docmodel STATIC
    ProvenanceTag.h
    Inline.h
    Inline.cpp
    Block.h
    Block.cpp
    SemanticDocument.h
    SemanticDocument.cpp
    DocumentFuzzer.h
    DocumentFuzzer.cpp
)

set_target_properties(docmodel PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_include_directories(docmodel PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
