# FindPdfium.cmake — locates the PDFium prebuilt headers + import lib.
# PDFium has no MSYS2 package; headers and import library are vendored
# under third_party/pdfium/ (copied from the prebuilt build).
#
# Sets: Pdfium_FOUND, Pdfium::Pdfium IMPORTED target

set(_pdfium_root "${CMAKE_SOURCE_DIR}/third_party/pdfium")

find_path(Pdfium_INCLUDE_DIR fpdfview.h
    PATHS "${_pdfium_root}/include"
    NO_DEFAULT_PATH)

find_library(Pdfium_LIBRARY NAMES pdfium libpdfium
    PATHS "${_pdfium_root}/lib"
    NO_DEFAULT_PATH)

if(Pdfium_LIBRARY)
    file(SHA256 "${Pdfium_LIBRARY}" _pdfium_hash)
    if(NOT _pdfium_hash STREQUAL "0fcd45dca1cb20e73f2335046d63f630afc13430e2d5e8309d08f6e940b0de03")
        message(FATAL_ERROR "PDFium binary checksum mismatch! Expected 0fcd45dca1cb20e73f2335046d63f630afc13430e2d5e8309d08f6e940b0de03 but got ${_pdfium_hash}")
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
