# === DEPENDENCIES ===
include(FetchContent)
cmake_policy(SET CMP0135 NEW)

# --- ArduinoMock Test Deps ---
FetchContent_Declare(
  ArduinoNativeMocks
  GIT_REPOSITORY https://github.com/Bytenana/ArduinoMock
  GIT_TAG rc-0.1.1
)
FetchContent_MakeAvailable(ArduinoNativeMocks)

# --- GoogleTest/GoogleMock (needed by ArduinoNativeMocks) ---
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP true
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Link pthread for POSIX port
find_package(Threads REQUIRED)

# --- Test-only dependencies ---
if(ASYNCAT_HANDLER_BUILD_TESTS_NATIVE)
  # --- Unity ---
  FetchContent_Declare(
    Unity
    URL https://github.com/ThrowTheSwitch/Unity/archive/refs/tags/v2.6.0.zip
  )
  FetchContent_MakeAvailable(Unity)
endif()
