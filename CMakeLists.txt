cmake_minimum_required(VERSION 3.25)
project(libpak LANGUAGES C VERSION 1.0)

option(LIBPAK_BUILD_TEST "whether test for libpak should be compiled" ${PROJECT_IS_TOP_LEVEL})

if(WIN32)
    add_compile_definitions(_CRT_SECURE_NO_DEPRECATE)
endif()

find_package(zstd REQUIRED)

add_library(libpak INTERFACE)
target_include_directories(libpak INTERFACE ${CMAKE_CURRENT_LIST_DIR}/)
target_link_libraries(libpak INTERFACE zstd::libzstd_static)

if(LIBPAK_BUILD_TEST)
    add_executable(libpaktest ${CMAKE_CURRENT_LIST_DIR}/test/libpaktest.c)
    target_link_libraries(libpaktest libpak)

    file(COPY ${CMAKE_CURRENT_LIST_DIR}/test/a.txt DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(COPY ${CMAKE_CURRENT_LIST_DIR}/test/b.txt DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(COPY ${CMAKE_CURRENT_LIST_DIR}/test/c.txt DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()