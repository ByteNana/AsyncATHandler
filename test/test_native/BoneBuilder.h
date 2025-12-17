#pragma once
#ifdef ESP32

#define ENV_BONES                           \
  void setup() {                                      \
    Serial.begin(115200);                             \
    ::testing::InitGoogleTest();                      \
  }                                                   \
                                                     \

  void loop() {                                       \
    if (RUN_ALL_TESTS())                              \
        ;                                               \
    delay(1000);                                      \
  }

#endif  // ESP32

#define ENV_BONES FREERTOS_TEST_MAIN()