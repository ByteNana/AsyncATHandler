cmake_minimum_required(VERSION 3.15)
project(AsyncATHandler CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Set build type to Debug for debugging symbols ---
set(CMAKE_BUILD_TYPE Debug)

# Config flag to differentiate between embedded vs native build
option(NATIVE_BUILD "Use mock Arduino headers for native testing" ON)

# Paths
set(LIB_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(MOCK_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test/mocks)
set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test)

# === NATIVE BUILD MODE ===
message(STATUS "🔧 Building with mock Arduino/FreeRTOS headers")

# Add mock includes
include_directories(
  ${MOCK_INC_DIR}
  ${LIB_SRC_DIR}
)

# === DEPENDENCIES (GMOCK & UNITY) ===
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


# --- Add dependency include directories ---
include_directories(
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

# === TESTS ===
if(ASYNCAT_HANDLER_BUILD_TESTS)
  enable_testing()

  # Get FreeRTOS.cpp from root and all .cpp files from subdirectories
  file(GLOB_RECURSE FREERTOS_SOURCES CONFIGURE_DEPENDS
    ${MOCK_INC_DIR}/freertos/FreeRTOS.cpp
    ${MOCK_INC_DIR}/freertos/*/*.cpp
  )

  # Only get test files from the TEST_DIR (not recursively from freertos)
  file(GLOB TEST_FILES CONFIGURE_DEPENDS
    ${TEST_DIR}/test_native/*.cpp
  )

  foreach(TEST_FILE ${TEST_FILES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    set(EXEC_NAME "${TEST_NAME}_test_exec")
    set(TEST_SRC ${TEST_FILE})
    list(APPEND TEST_SRC ${FREERTOS_SOURCES})

    add_executable(${EXEC_NAME} ${TEST_SRC})
    target_link_libraries(${EXEC_NAME}
      PRIVATE AsyncATHandler gmock gtest gtest_main unity
    )
    add_test(NAME ${EXEC_NAME} COMMAND ${EXEC_NAME})
  endforeach()
endif()
