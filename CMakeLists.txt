cmake_minimum_required(VERSION 3.15)
project(AsyncATHandler C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Set build type to Debug for debugging symbols ---
set(CMAKE_BUILD_TYPE Debug)

# Config flag to differentiate between embedded vs native build
option(NATIVE_BUILD "Use mock Arduino headers for native testing" ON)
option(ASYNCAT_HANDLER_BUILD_TESTS "Build tests" ON)
option(CLANGD_INDEX_EXAMPLES "Add example sources to compile_commands.json" ON)

# Paths
set(LIB_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(MOCK_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test/mocks)
set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test)

# === NATIVE BUILD MODE ===
message(STATUS "🔧 Building with mock Arduino/FreeRTOS headers")

# === DEPENDENCIES ===
include(FetchContent)
cmake_policy(SET CMP0135 NEW)

# --- GoogleTest/GoogleMock ---
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP true
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# --- Unity ---
FetchContent_Declare(
  Unity
  URL https://github.com/ThrowTheSwitch/Unity/archive/refs/tags/v2.6.0.zip
)
FetchContent_MakeAvailable(Unity)

# === LOCAL FREERTOS SHIM (no network) ===
file(GLOB FREERTOS_SHIM_SOURCES
  ${TEST_DIR}/mocks/freertos/*.cpp
  ${TEST_DIR}/mocks/freertos/*.c
)

add_library(FreeRTOS_Sim_Lib STATIC ${FREERTOS_SHIM_SOURCES})

target_include_directories(FreeRTOS_Sim_Lib PUBLIC
  ${MOCK_INC_DIR}
  ${MOCK_INC_DIR}/freertos
)

find_package(Threads REQUIRED)
target_link_libraries(FreeRTOS_Sim_Lib PUBLIC Threads::Threads)

# === INCLUDE DIRECTORIES ===
include_directories(
  ${MOCK_INC_DIR}
  ${LIB_SRC_DIR}
  ${MOCK_INC_DIR}/freertos
  ${gmock_SOURCE_DIR}/include
  ${gtest_SOURCE_DIR}/include
  ${Unity_SOURCE_DIR}/src
)

# === LIBRARY ===
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
  ${LIB_SRC_DIR}/*.cpp
  ${LIB_SRC_DIR}/*.h
)

add_library(AsyncATHandler STATIC ${SRC_FILES})

# Link FreeRTOS-Sim to your main library so all files can use it
target_link_libraries(AsyncATHandler PUBLIC FreeRTOS_Sim_Lib)

enable_language(C)

set(LOG_LEVEL "3" CACHE STRING "Log level (0-5, where 5 is most verbose)")
set_property(CACHE LOG_LEVEL PROPERTY STRINGS "0" "1" "2" "3" "4" "5")

# === TESTS ===
if(ASYNCAT_HANDLER_BUILD_TESTS)
  add_compile_definitions(LOG_LEVEL=${LOG_LEVEL})
  enable_testing()

  # Get any existing mock FreeRTOS sources (keeping for backward compatibility)
  file(GLOB_RECURSE EXISTING_FREERTOS_SOURCES CONFIGURE_DEPENDS
    ${MOCK_INC_DIR}/freertos/FreeRTOS.cpp
    ${MOCK_INC_DIR}/freertos/**/*.cpp
    ${MOCK_INC_DIR}/freertos/**/*.c
  )

  # Only get test files from the TEST_DIR
  file(GLOB TEST_FILES CONFIGURE_DEPENDS
    ${TEST_DIR}/test_native/*.cpp
  )

  foreach(TEST_FILE ${TEST_FILES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    set(EXEC_NAME "${TEST_NAME}_test_exec")

    set(TEST_SRC ${TEST_FILE})
    # Add existing mock sources if they exist
    if(EXISTING_FREERTOS_SOURCES)
      list(APPEND TEST_SRC ${EXISTING_FREERTOS_SOURCES})
    endif()

    add_executable(${EXEC_NAME} ${TEST_SRC} ${MOCK_INC_DIR}/freertos/freertos_hooks.c)

    target_link_libraries(${EXEC_NAME}
      PRIVATE
      AsyncATHandler
      FreeRTOS_Sim_Lib
      gmock
      gtest
      gtest_main
      unity
    )

    add_test(NAME ${EXEC_NAME} COMMAND ${EXEC_NAME})
  endforeach()
endif()

# === CLANGD: index example sources without building ===
if(CLANGD_INDEX_EXAMPLES)
  file(GLOB_RECURSE EXAMPLE_MAIN_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/examples/*/src/*.cpp
  )

  foreach(EX_FILE ${EXAMPLE_MAIN_SOURCES})
    # Derive a readable target name from examples/<name>
    get_filename_component(_src_dir ${EX_FILE} DIRECTORY)
    get_filename_component(_example_dir ${_src_dir} DIRECTORY)
    get_filename_component(_example_name ${_example_dir} NAME)
    set(TGT clangd_example_${_example_name})

    # OBJECT + EXCLUDE_FROM_ALL ensures entries in compile_commands.json without adding build targets
    add_library(${TGT} OBJECT EXCLUDE_FROM_ALL ${EX_FILE})
    target_include_directories(${TGT} PRIVATE
      ${LIB_SRC_DIR}
      ${MOCK_INC_DIR}
      ${MOCK_INC_DIR}/freertos
    )
    target_compile_definitions(${TGT} PRIVATE LOG_LEVEL=${LOG_LEVEL})
  endforeach()
endif()
