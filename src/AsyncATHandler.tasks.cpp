#include "AsyncATHandler.h"

#include <esp_log.h>

void AsyncATHandler::readerTaskFunction(void* parameter) {
  AsyncATHandler* handler = static_cast<AsyncATHandler*>(parameter);

  while (true) {
    ATCommand cmd;

    // Check for commands to send
    if (xSemaphoreTake(handler->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      if (xQueueReceive(handler->commandQueue, &cmd, pdMS_TO_TICKS(0)) == pdTRUE) {
        // Send the command to the stream
        handler->_stream->println(cmd.command);
        handler->_stream->flush();

        log_d("Sent command: '%s'", cmd.command);

        // Clean up semaphore if it exists (shouldn't for async commands)
        if (cmd.responseSemaphore) {
          vSemaphoreDelete(cmd.responseSemaphore);
        }
      }
      xSemaphoreGive(handler->mutex);
    }

    // Always process incoming data
    handler->processIncomingData();

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
