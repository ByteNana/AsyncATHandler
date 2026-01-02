#pragma once
#ifdef ESP32

#define ENV_BONES_SETUP()        \
  void setup() {                 \
    Serial.begin(115200);        \
    ::testing::InitGoogleTest(); \
    RUN_ALL_TESTS();             \
  }                              \

#define ENV_BONES_LOOP()                     \
  void loop() {                              \
  }

#define ENV_BONES() \
  ENV_BONES_SETUP() \
  ENV_BONES_LOOP()


#define NATIVE_ONLY(test)
#define HARDWARE_ONLY(test) (test)

#else /* native */

#define ENV_BONES() FREERTOS_TEST_MAIN()

#define NATIVE_ONLY(test) test
#define HARDWARE_ONLY(test)

#endif  // ESP32