# === TESTS ===
if(ASYNCAT_HANDLER_BUILD_TESTS)
  add_compile_definitions(LOG_LEVEL=${LOG_LEVEL})
  enable_testing()

  # Only get test files from the TEST_DIR
  file(GLOB TEST_FILES CONFIGURE_DEPENDS
    ${TEST_DIR}/test_native/*.cpp
  )

  foreach(TEST_FILE ${TEST_FILES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    set(EXEC_NAME "${TEST_NAME}_test_exec")

    set(TEST_SRC ${TEST_FILE})

    add_executable(${EXEC_NAME} ${TEST_SRC})

    target_link_libraries(${EXEC_NAME}
      PRIVATE
      ArduinoNativeMocks
      AsyncATHandler
      gmock
      gtest
      gtest_main
      unity
    )

    add_test(NAME ${EXEC_NAME} COMMAND ${EXEC_NAME})
  endforeach()
endif()
