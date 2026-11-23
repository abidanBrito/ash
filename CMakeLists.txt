cmake_minimum_required(VERSION 3.25)
project(
    ash
    VERSION 0.1.0
    DESCRIPTION "Minimal, POSIX compliant shell implementation"
    HOMEPAGE_URL "https://github.com/abidanBrito/ash"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(READLINE REQUIRED IMPORTED_TARGET readline)

add_executable(ash
    src/main.cpp
    src/shell.cpp
    src/commands.cpp
    src/parser.cpp
)

target_compile_options(ash PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(ash PRIVATE PkgConfig::READLINE)
