
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was podofo-config.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

####################################################################################

if("OFF")
    include(CMakeFindDependencyMacro)
    if("ON")
        find_dependency(utf8proc CONFIG)
    endif()
    if("TRUE")
        find_dependency(Fontconfig)
    endif()
    find_dependency(Freetype)
    if("TRUE")
        find_dependency(JPEG)
    endif()
    if("")
        find_dependency(LCMS2)
    endif()
    find_dependency(LibXml2)
    find_dependency(OpenSSL)
    if("TRUE")
        find_dependency(PNG)
    endif()
    if("TRUE")
        find_dependency(TIFF)
    endif()
    find_dependency(ZLIB)
endif()

include ("${CMAKE_CURRENT_LIST_DIR}/podofo-targets.cmake")

add_library(podofo::podofo ALIAS podofo::podofo_shared)
