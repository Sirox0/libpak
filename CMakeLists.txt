cmake_minimum_required(VERSION 3.25)
project(libpak LANGUAGES C VERSION 1.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(SOURCES
    #hashing library
    ${CMAKE_CURRENT_LIST_DIR}/external/sha1/sha1.c

    ${CMAKE_CURRENT_LIST_DIR}/src/compressor.c
    ${CMAKE_CURRENT_LIST_DIR}/src/decompressor.c
    ${CMAKE_CURRENT_LIST_DIR}/src/reader.c
)

find_package(libdeflate REQUIRED)

option(LIBPAK_STATIC_LIBRARY "of set to on, static library is produced, shared library is produced otherwise" OFF)
if(LIBPAK_STATIC_LIBRARY)
    add_library(pak STATIC ${SOURCES})
else()
    add_library(pak SHARED ${SOURCES})
endif()
target_link_libraries(pak libdeflate::libdeflate_static)
target_include_directories(pak PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include ${CMAKE_CURRENT_LIST_DIR}/external/sha1)
