cmake_minimum_required(VERSION 3.10)
project(termux-am-socket)


if (CMAKE_INSTALL_PREFIX STREQUAL "/usr/local")
    set(CMAKE_INSTALL_PREFIX $ENV{PREFIX})
    message(STATUS "prefix set from env: " ${CMAKE_INSTALL_PREFIX})
endif()



add_compile_definitions(PREFIX="${CMAKE_INSTALL_PREFIX}")

add_executable(termux-am termux-am.c)

install(TARGETS termux-am DESTINATION bin PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

