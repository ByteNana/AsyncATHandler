#include <stdio.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"

void vApplicationIdleHook(void) {}

void vApplicationTickHook(void) {}

void vApplicationMallocFailedHook(void) {
  fprintf(stderr, "FreeRTOS: Malloc failed - Free heap: %zu bytes\n", xPortGetFreeHeapSize());
  abort();
}

/* Match your configASSERT mapping. If your FreeRTOSConfig.h maps configASSERT
   to vAssertCalled(file,line), keep this signature; otherwise use void vAssertCalled(void). */
void vAssertCalled(unsigned long ulLine, const char* const pcFileName) {
  fprintf(stderr, "FreeRTOS assert at %s:%lu\n", pcFileName, ulLine);
  abort();
}
