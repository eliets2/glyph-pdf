# fuzz/CMakeLists.txt
#
# Fuzzing / verification apparatus for GlyphPDF. This is a SUBDIRECTORY project
# that reuses the parent's pdfws_engines target so it inherits every transitive
# dependency (PoDoFo, Qt, OpenSSL, PDFium, ...) without hand-rolling link order.
#
# It is NOT wired into the root build. To enable, the OWNER adds ONE line to the
# root CMakeLists.txt (we do not edit it for you):
#
#     add_subdirectory(fuzz)
#
# ...guarded however you like, e.g.  if(GLYPHPDF_FUZZ) add_subdirectory(fuzz) endif()
#
# Then configure a SEPARATE build dir (never reuse the main build/):
#   cmake -S . -B fuzz/build -G Ninja -DGLYPHPDF_FUZZ=ON <same -D flags as main build>
#   cmake --build fuzz/build --target redaction_driver
#
# Produces: fuzz/build/fuzz/redaction_driver(.exe)
#
# The redaction_driver links the REAL engine and is what the Python oracles in
# fuzz/oracle/ consume. It is a headless QCoreApplication (no GUI, no QApplication).

cmake_minimum_required(VERSION 3.21)

# --- P1/P2 redaction + Tm driver (links the real engine) --------------------
if(TARGET pdfws_engines)
    add_executable(redaction_driver harnesses/redaction_driver.cpp)
    target_link_libraries(redaction_driver PRIVATE pdfws_engines)
    # engine is PUBLIC-linked to pdfws_core (Qt etc); also need Core explicitly
    find_package(Qt6 COMPONENTS Core REQUIRED)
    target_link_libraries(redaction_driver PRIVATE Qt6::Core)
    set_target_properties(redaction_driver PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
else()
    message(WARNING "[fuzz] pdfws_engines target not found; redaction_driver skipped. "
                    "Configure from the repo root with add_subdirectory(fuzz).")
endif()

# --- P3 djot libFuzzer harness (REQUIRES clang + compiler-rt) ---------------
# Guarded behind GLYPHPDF_FUZZ_LIBFUZZER because GCC has no -fsanitize=fuzzer.
# When building with clang, also rebuild liblua/pdfws_djot/docmodel with clang
# (ABI). See fuzz/build_clang/build_djot_clang.sh for the standalone recipe.
if(GLYPHPDF_FUZZ_LIBFUZZER AND TARGET pdfws_djot AND TARGET docmodel AND TARGET liblua)
    add_executable(djot_fuzzer harnesses/harness_djot.cpp)
    target_compile_options(djot_fuzzer PRIVATE
        -g -O1 -fno-omit-frame-pointer -fsanitize=fuzzer,address,undefined)
    target_link_options(djot_fuzzer PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_compile_definitions(djot_fuzzer PRIVATE
        DJOT_LIB_PATH="${CMAKE_SOURCE_DIR}/third_party/djot")
    target_include_directories(djot_fuzzer PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/pdfws_djot
        ${CMAKE_SOURCE_DIR}/third_party/lua-5.4/src)
    target_link_libraries(djot_fuzzer PRIVATE pdfws_djot docmodel liblua)
    set_target_properties(djot_fuzzer PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endif()
