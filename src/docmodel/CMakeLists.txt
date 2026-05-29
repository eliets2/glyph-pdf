cmake_minimum_required(VERSION 3.20)
project(docmodel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

target_include_directories(docmodel PUBLIC )
