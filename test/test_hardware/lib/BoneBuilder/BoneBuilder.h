#pragma once
#ifdef ESP32

#define ENV_BONES()

#define NATIVE_ONLY(test)
#define HARDWARE_ONLY(test) (test)

#else /* native */

#define ENV_BONES() FREERTOS_TEST_MAIN()

#define NATIVE_ONLY(test) test
#define HARDWARE_ONLY(test)

#endif  // ESP32