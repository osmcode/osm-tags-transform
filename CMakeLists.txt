#------------------------------------------------------------------------------
#
#  CMakeLists.txt
#
#------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.5.0)

project(osm-tags-transform VERSION 0.0.1 LANGUAGES CXX C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(BUILD_TESTS "Build test suite" OFF)
option(WITH_LUAJIT "Build with LuaJIT support" OFF)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()


#------------------------------------------------------------------------------
#
#  Libraries
#
#------------------------------------------------------------------------------

find_package(Osmium 2.13 REQUIRED COMPONENTS io)
include_directories(SYSTEM ${OSMIUM_INCLUDE_DIRS} ${PROTOZERO_INCLUDE_DIR})
set(LIBS ${OSMIUM_LIBRARIES})

if (WITH_LUAJIT)
    find_package(LuaJIT REQUIRED)
    include_directories(SYSTEM ${LUAJIT_INCLUDE_DIR})
    list(APPEND LIBS ${LUAJIT_LIBRARIES})
    add_definitions(-DHAVE_LUAJIT=1)
else()
    find_package(Lua REQUIRED)
    include_directories(SYSTEM ${LUA_INCLUDE_DIR})
    list(APPEND LIBS ${LUA_LIBRARIES})
endif()

find_package(Threads REQUIRED)

message(STATUS "Libraries used to build: ${LIBS}")

add_compile_options(-Wall -Wextra)

add_definitions(-DPROJECT_VERSION="${PROJECT_VERSION}")

add_subdirectory(src)


#------------------------------------------------------------------------------
#
#  clang-tidy
#
#------------------------------------------------------------------------------

message(STATUS "Looking for clang-tidy")
find_program(CLANG_TIDY
             NAMES clang-tidy clang-tidy-14 clang-tidy-13 clang-tidy-12 clang-tidy-11)

if (CLANG_TIDY)
    message(STATUS "Looking for clang-tidy - found ${CLANG_TIDY}")

    file(GLOB CT_CHECK_FILES src/*.cpp)

    add_custom_target(clang-tidy
        ${CLANG_TIDY}
        -p ${CMAKE_BINARY_DIR}
        ${CT_CHECK_FILES}
    )
else()
    message(STATUS "Looking for clang-tidy - not found")
    message(STATUS "  Build target 'clang-tidy' will not be available.")
endif()


#------------------------------------------------------------------------------
#
#  Tests
#
#------------------------------------------------------------------------------

if (BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
else()
    message(STATUS "Tests disabled. Set BUILD_TESTS=ON to enable tests.")
endif()


#------------------------------------------------------------------------------
