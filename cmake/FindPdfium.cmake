# FindPdfium.cmake
# Find PDFium headers and library
#
# Output variables:
#   Pdfium_FOUND: Whether the library was found
#   Pdfium_INCLUDE_DIRS: Include directories
#   Pdfium_LIBRARIES: Libraries to link against

find_path(Pdfium_INCLUDE_DIR
    NAMES fpdfview.h
    PATHS
        ${CMAKE_PREFIX_PATH}
        "C:/vcpkg/installed/x64-mingw-dynamic"
    PATH_SUFFIXES include
)

find_library(Pdfium_LIBRARY
    NAMES pdfium libpdfium.dll.a pdfium.dll.lib
    PATHS
        ${CMAKE_PREFIX_PATH}
        "C:/vcpkg/installed/x64-mingw-dynamic"
    PATH_SUFFIXES lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Pdfium
    REQUIRED_VARS Pdfium_LIBRARY Pdfium_INCLUDE_DIR
)

if(Pdfium_FOUND)
    set(Pdfium_INCLUDE_DIRS ${Pdfium_INCLUDE_DIR})
    set(Pdfium_LIBRARIES ${Pdfium_LIBRARY})

    if(NOT TARGET Pdfium::Pdfium)
        add_library(Pdfium::Pdfium UNKNOWN IMPORTED)
        set_target_properties(Pdfium::Pdfium PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${Pdfium_INCLUDE_DIRS}"
            IMPORTED_LOCATION "${Pdfium_LIBRARY}"
        )
    endif()
endif()
