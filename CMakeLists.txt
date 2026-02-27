cmake_minimum_required(VERSION 3.15)
project(AsyncATHandler VERSION 0.1.1 LANGUAGES C CXX)
include(cmake/compiler_settings.cmake)

set(PROJECT_VERSION_SUFIX "-rc.1")
set(CMAKE_PROJECT_VERSION "${PROJECT_VERSION}${PROJECT_VERSION_SUFIX}")

# Config flag to differentiate between embedded vs native build
option(NATIVE_BUILD "Use mock Arduino headers for native testing" ON)
option(ASYNCAT_HANDLER_BUILD_TESTS_NATIVE "Build native test executables" OFF)
option(CLANGD_INDEX_EXAMPLES "Add example sources to compile_commands.json" ON)

# Paths
set(LIB_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test)

include(cmake/dependencies.cmake)

# === INCLUDE DIRECTORIES ===
include_directories(
  ${LIB_SRC_DIR}
  ${gmock_SOURCE_DIR}/include
  ${gtest_SOURCE_DIR}/include
)

set(LOG_LEVEL "3" CACHE STRING "Log level (0-5, where 5 is most verbose)")
set_property(CACHE LOG_LEVEL PROPERTY STRINGS "0" "1" "2" "3" "4" "5")
add_compile_definitions(LOG_LEVEL=${LOG_LEVEL})

# === LIBRARY ===
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
  ${LIB_SRC_DIR}/*.cpp
  ${LIB_SRC_DIR}/*.h
)

add_library(AsyncATHandler STATIC ${SRC_FILES})

target_link_libraries(AsyncATHandler PUBLIC ArduinoNativeMocks)

enable_language(C)

include(cmake/utils.cmake)
