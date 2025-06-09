cmake_minimum_required(VERSION 3.25)
project(libpak LANGUAGES C VERSION 1.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O3")

if(WIN32)
    add_compile_definitions(_CRT_SECURE_NO_DEPRECATE)
endif()

set(SOURCES
    #hashing library
    ${CMAKE_CURRENT_LIST_DIR}/external/sha256/sha256.c

    ${CMAKE_CURRENT_LIST_DIR}/src/compressor.c
    ${CMAKE_CURRENT_LIST_DIR}/src/decompressor.c
    ${CMAKE_CURRENT_LIST_DIR}/src/reader.c
)

find_package(libdeflate REQUIRED)

option(LIBPAK_STATIC "produce static library" ON)
if(LIBPAK_STATIC)
    add_library(pak STATIC ${SOURCES})
    add_compile_definitions(PAK_STATIC)
else()
    add_library(pak SHARED ${SOURCES})
endif()
target_link_libraries(pak libdeflate::libdeflate_static)
target_include_directories(pak PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include ${CMAKE_CURRENT_LIST_DIR}/external/sha256)

include(GenerateExportHeader)
generate_export_header(pak EXPORT_FILE_NAME ${CMAKE_CURRENT_LIST_DIR}/include/libpakExport.h STATIC_DEFINE PAK_STATIC)