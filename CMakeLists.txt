cmake_minimum_required(VERSION 3.10)
project(termux-am-socket)

if (CMAKE_INSTALL_PREFIX STREQUAL "/usr/local")
    set(CMAKE_INSTALL_PREFIX $ENV{PREFIX})
    message(STATUS "prefix set from env: " ${CMAKE_INSTALL_PREFIX})
endif()

set(TERMUX_PREFIX ${CMAKE_INSTALL_PREFIX})

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/termux-am.h.in
    termux-am.h @ONLY
)

add_executable(termux-am termux-am.c)
target_include_directories(termux-am PUBLIC ${CMAKE_BINARY_DIR})

install(TARGETS termux-am DESTINATION bin PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
