# FindPdfium.cmake — locates the PDFium prebuilt headers + import lib.
# PDFium has no MSYS2 package; headers and import library are vendored
# under third_party/pdfium/ (copied from the prebuilt build).
#
# Sets: Pdfium_FOUND, Pdfium::Pdfium IMPORTED target

set(_pdfium_root "${CMAKE_SOURCE_DIR}/third_party/pdfium")

# L03 (native-Linux artifact manifest): the pin is per-binary. Windows links
# the vendored MinGW import stub (lib/libpdfium.dll.a) against the staged
# bin/pdfium.dll; Linux links the prebuilt shared object (lib/libpdfium.so)
# from the same bblanchon release (chromium/7834) directly. Both hashes are
# enforced at configure time — the 7z-lane discipline (see
# third_party/7zip/PROVENANCE.md and third_party/pdfium/PROVENANCE.md).

find_path(Pdfium_INCLUDE_DIR fpdfview.h
    PATHS "${_pdfium_root}/include"
    NO_DEFAULT_PATH)

find_library(Pdfium_LIBRARY NAMES pdfium libpdfium
    PATHS "${_pdfium_root}/lib"
    NO_DEFAULT_PATH)

if(Pdfium_LIBRARY)
    file(SHA256 "${Pdfium_LIBRARY}" _pdfium_hash)
    if(WIN32)
        set(_pdfium_expected_hash "0fcd45dca1cb20e73f2335046d63f630afc13430e2d5e8309d08f6e940b0de03") # lib/libpdfium.dll.a (win-x64, chromium/7834)
    else()
        set(_pdfium_expected_hash "246872bdd5e05843b70051e6378216cc584535a1f4a7248b9f88059715d70f7c") # lib/libpdfium.so (linux-x64, chromium/7834)
    endif()
    if(NOT _pdfium_hash STREQUAL _pdfium_expected_hash)
        message(FATAL_ERROR "PDFium binary checksum mismatch! Expected ${_pdfium_expected_hash} but got ${_pdfium_hash}. See third_party/pdfium/PROVENANCE.md and scripts/bootstrap-vendor-deps.sh.")
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Pdfium DEFAULT_MSG Pdfium_LIBRARY Pdfium_INCLUDE_DIR)

if(Pdfium_FOUND AND NOT TARGET Pdfium::Pdfium)
    add_library(Pdfium::Pdfium UNKNOWN IMPORTED)
    set_target_properties(Pdfium::Pdfium PROPERTIES
        IMPORTED_LOCATION "${Pdfium_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${Pdfium_INCLUDE_DIR}")
endif()

mark_as_advanced(Pdfium_INCLUDE_DIR Pdfium_LIBRARY)
