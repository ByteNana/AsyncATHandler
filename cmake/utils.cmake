# === TESTS ===
if(ASYNCAT_HANDLER_BUILD_TESTS_NATIVE)
  include_directories(
    ${gmock_SOURCE_DIR}/include
    ${gtest_SOURCE_DIR}/include
    ${Unity_SOURCE_DIR}/src
  )

  enable_testing()

  # Only glob test_*.cpp to exclude main.cpp (ESP32-only entry point)
  file(GLOB TEST_FILES CONFIGURE_DEPENDS
    ${TEST_DIR}/test/test_*.cpp
  )

  foreach(TEST_FILE ${TEST_FILES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    set(EXEC_NAME "${TEST_NAME}_test_exec")

    add_executable(${EXEC_NAME} ${TEST_FILE})

    target_include_directories(${EXEC_NAME}
      PRIVATE
      ${TEST_DIR}
      ${TEST_DIR}/lib/BoneBuilder
      ${TEST_DIR}/lib/Mocks
      ${TEST_DIR}/lib/SerialCommunicator
      ${TEST_DIR}/lib/TestCommon
    )

    target_link_libraries(${EXEC_NAME}
      PRIVATE
      AsyncATHandler
      gmock
      gtest_main
      unity
    )

    add_test(NAME ${EXEC_NAME} COMMAND ${EXEC_NAME})
  endforeach()
endif()
