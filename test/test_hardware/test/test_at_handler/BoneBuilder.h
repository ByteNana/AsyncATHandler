#define ESP32 1
#pragma once
#ifdef ESP32

#define ENV_BONES_SETUP()        \
  void setup() {                 \
    Serial.begin(115200);        \
    ::testing::InitGoogleTest(); \
  }

#define ENV_BONES_LOOP() \
  void loop() {          \
    RUN_ALL_TESTS();     \
    delay(1000);         \
  }

#define ENV_BONES() \
  ENV_BONES_SETUP() \
  ENV_BONES_LOOP()

#else

#define ENV_BONES() FREERTOS_TEST_MAIN()

#endif  // ESP32