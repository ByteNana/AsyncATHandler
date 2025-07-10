// test/minimal_freertos_mock.cpp
#include <gmock/gmock.h>  // Not strictly needed for this test, but often used with mocks
#include <gtest/gtest.h>

#include <atomic>  // For std::atomic
#include <chrono>
#include <cstring>  // For strcmp if comparing char arrays
#include <iostream>
#include <thread>

// Include your FreeRTOS mock header (which is already type-agnostic)
#include "freertos/FreeRTOS.h"

// No ATHandler.settings.h or Arduino.h included here!
// We will use fundamental types or simple structs for testing.

// Define a simple struct for testing queue with custom data
struct TestMessage {
  int id;
  char name[32];  // Use char array for string-like data without String class
  float value;

  // Helper for comparison in tests
  bool operator==(const TestMessage& other) const {
    return id == other.id && std::strcmp(name, other.name) == 0 && value == other.value;
  }
};

// Test fixture for FreeRTOS mocks
class FreeRTOSMockTest : public ::testing::Test {
 protected:
  void SetUp() override { std::cout << "\n--- FreeRTOSMockTest SetUp ---" << std::endl; }

  void TearDown() override {
    std::cout << "--- FreeRTOSMockTest TearDown ---" << std::endl;
    // Individual tests should clean up their created handles.
  }

  // Helper to simulate FreeRTOS delay
  void SimulateDelay(TickType_t ticks) { vTaskDelay(ticks); }
};

// --- Test 1: Basic Queue Creation and Deletion ---
TEST_F(FreeRTOSMockTest, QueueCreationAndDeletion) {
  std::cout << "Running Test: QueueCreationAndDeletion" << std::endl;
  // Create a queue for TestMessage structs
  QueueHandle_t msgQueue = xQueueCreate(10, sizeof(TestMessage));
  ASSERT_NE(msgQueue, nullptr) << "Message queue creation failed!";

  // Create a queue for a basic integer type
  QueueHandle_t intQueue = xQueueCreate(5, sizeof(int));
  ASSERT_NE(intQueue, nullptr) << "Integer queue creation failed!";

  // Check initial state
  EXPECT_EQ(uxQueueMessagesWaiting(msgQueue), 0);
  EXPECT_EQ(uxQueueMessagesWaiting(intQueue), 0);

  // Clean up
  vQueueDelete(msgQueue);
  vQueueDelete(intQueue);
}

// --- Test 2: Basic Send and Receive (Non-Blocking) with int ---
TEST_F(FreeRTOSMockTest, QueueSendReceiveIntNonBlocking) {
  std::cout << "Running Test: QueueSendReceiveIntNonBlocking" << std::endl;
  QueueHandle_t intQueue = xQueueCreate(2, sizeof(int));
  ASSERT_NE(intQueue, nullptr);

  int val1 = 10;
  int val2 = 20;
  int val3 = 30;
  int receivedVal;

  // Send val1 - should succeed
  EXPECT_EQ(xQueueSend(intQueue, &val1, 0), pdTRUE);
  EXPECT_EQ(uxQueueMessagesWaiting(intQueue), 1);

  // Send val2 - should succeed
  EXPECT_EQ(xQueueSend(intQueue, &val2, 0), pdTRUE);
  EXPECT_EQ(uxQueueMessagesWaiting(intQueue), 2);

  // Send val3 (queue is full) - should fail (non-blocking)
  EXPECT_EQ(xQueueSend(intQueue, &val3, 0), pdFALSE);
  EXPECT_EQ(uxQueueMessagesWaiting(intQueue), 2);  // Still 2

  // Receive val1 - should succeed
  EXPECT_EQ(xQueueReceive(intQueue, &receivedVal, 0), pdTRUE);
  EXPECT_EQ(receivedVal, 10);
  EXPECT_EQ(uxQueueMessagesWaiting(intQueue), 1);

  // Receive val2 - should succeed
  EXPECT_EQ(xQueueReceive(intQueue, &receivedVal, 0), pdTRUE);
  EXPECT_EQ(receivedVal, 20);
  EXPECT_EQ(uxQueueMessagesWaiting(intQueue), 0);

  // Receive from empty queue - should fail (non-blocking)
  EXPECT_EQ(xQueueReceive(intQueue, &receivedVal, 0), pdFALSE);
  EXPECT_EQ(uxQueueMessagesWaiting(intQueue), 0);

  vQueueDelete(intQueue);
}

// --- Test 3: Basic Send and Receive (Non-Blocking) with TestMessage struct ---
TEST_F(FreeRTOSMockTest, QueueSendReceiveStructNonBlocking) {
  std::cout << "Running Test: QueueSendReceiveStructNonBlocking" << std::endl;
  QueueHandle_t msgQueue = xQueueCreate(2, sizeof(TestMessage));
  ASSERT_NE(msgQueue, nullptr);

  TestMessage msg1 = {1, "Test Message One", 1.1f};
  TestMessage msg2 = {2, "Second Message", 2.2f};
  TestMessage msg3 = {3, "Third Message", 3.3f};
  TestMessage receivedMsg;

  // Send msg1 - should succeed
  EXPECT_EQ(xQueueSend(msgQueue, &msg1, 0), pdTRUE);
  EXPECT_EQ(uxQueueMessagesWaiting(msgQueue), 1);

  // Send msg2 - should succeed
  EXPECT_EQ(xQueueSend(msgQueue, &msg2, 0), pdTRUE);
  EXPECT_EQ(uxQueueMessagesWaiting(msgQueue), 2);

  // Send msg3 (queue is full) - should fail (non-blocking)
  EXPECT_EQ(xQueueSend(msgQueue, &msg3, 0), pdFALSE);
  EXPECT_EQ(uxQueueMessagesWaiting(msgQueue), 2);  // Still 2

  // Receive msg1 - should succeed
  EXPECT_EQ(xQueueReceive(msgQueue, &receivedMsg, 0), pdTRUE);
  EXPECT_EQ(receivedMsg, msg1);  // Uses operator==
  EXPECT_EQ(uxQueueMessagesWaiting(msgQueue), 1);

  // Receive msg2 - should succeed
  EXPECT_EQ(xQueueReceive(msgQueue, &receivedMsg, 0), pdTRUE);
  EXPECT_EQ(receivedMsg, msg2);
  EXPECT_EQ(uxQueueMessagesWaiting(msgQueue), 0);

  // Receive from empty queue - should fail (non-blocking)
  EXPECT_EQ(xQueueReceive(msgQueue, &receivedMsg, 0), pdFALSE);
  EXPECT_EQ(uxQueueMessagesWaiting(msgQueue), 0);

  vQueueDelete(msgQueue);
}

// --- Test 4: Send and Receive with Blocking (Timeouts) for int ---
TEST_F(FreeRTOSMockTest, QueueSendReceiveBlockingWithTimeoutInt) {
  std::cout << "Running Test: QueueSendReceiveBlockingWithTimeoutInt" << std::endl;
  QueueHandle_t intQueue = xQueueCreate(1, sizeof(int));
  ASSERT_NE(intQueue, nullptr);

  int val1 = 100;
  int val2 = 200;
  int receivedVal;

  // Send val1 - should succeed
  EXPECT_EQ(xQueueSend(intQueue, &val1, 0), pdTRUE);
  EXPECT_EQ(uxQueueMessagesWaiting(intQueue), 1);

  std::atomic<bool> sendAttempted(false);
  std::atomic<bool> sendSucceeded(false);

  // Thread trying to send to a full queue with a timeout
  std::thread senderThread([&]() {
    sendAttempted = true;
    // This will block for a short time or until queue space is available
    sendSucceeded = (xQueueSend(intQueue, &val2, pdMS_TO_TICKS(50)) == pdTRUE);  // 50ms timeout
  });

  // Wait a moment for the sender thread to try sending
  SimulateDelay(10);  // Give the sender thread a chance to run and block
  EXPECT_TRUE(sendAttempted);
  EXPECT_FALSE(sendSucceeded);  // Should still be false, as queue is full

  // Now, receive an item, which should free up space
  EXPECT_EQ(xQueueReceive(intQueue, &receivedVal, 0), pdTRUE);
  EXPECT_EQ(receivedVal, 100);
  EXPECT_EQ(uxQueueMessagesWaiting(intQueue), 0);  // Queue is now empty

  // Give sender thread a chance to complete
  SimulateDelay(10);    // Let the sender thread wake up and send
  senderThread.join();  // Wait for the sender thread to finish its operation

  // Sender should now have succeeded because space became available
  EXPECT_TRUE(sendSucceeded);
  EXPECT_EQ(uxQueueMessagesWaiting(intQueue), 1);  // Now has val2

  EXPECT_EQ(xQueueReceive(intQueue, &receivedVal, 0), pdTRUE);
  EXPECT_EQ(receivedVal, 200);

  vQueueDelete(intQueue);
}

// --- Test 5: Basic Semaphore Creation and Usage ---
TEST_F(FreeRTOSMockTest, SemaphoreBinaryBasic) {
  std::cout << "Running Test: SemaphoreBinaryBasic" << std::endl;
  SemaphoreHandle_t sem = xSemaphoreCreateBinary();
  ASSERT_NE(sem, nullptr);

  // Initially, a binary semaphore taken without prior give should fail (or block if timeout > 0)
  EXPECT_EQ(xSemaphoreTake(sem, 0), pdFALSE);

  // Give the semaphore
  EXPECT_EQ(xSemaphoreGive(sem), pdTRUE);

  // Take the semaphore - should succeed
  EXPECT_EQ(xSemaphoreTake(sem, 0), pdTRUE);

  // Try taking again - should fail (semaphore already taken)
  EXPECT_EQ(xSemaphoreTake(sem, 0), pdFALSE);

  vSemaphoreDelete(sem);
}

// --- Test 6: Semaphore with Blocking Take and Give from Another Thread ---
TEST_F(FreeRTOSMockTest, SemaphoreBlockingTake) {
  std::cout << "Running Test: SemaphoreBlockingTake" << std::endl;
  SemaphoreHandle_t sem = xSemaphoreCreateBinary();
  ASSERT_NE(sem, nullptr);

  std::atomic<bool> takeAttempted(false);
  std::atomic<bool> takeSucceeded(false);

  // Thread trying to take a semaphore with a timeout
  std::thread takerThread([&]() {
    takeAttempted = true;
    takeSucceeded = (xSemaphoreTake(sem, pdMS_TO_TICKS(100)) == pdTRUE);  // 100ms timeout
  });

  // Wait a moment for the taker thread to try taking
  SimulateDelay(10);  // Give the taker thread a chance to run and block
  EXPECT_TRUE(takeAttempted);
  EXPECT_FALSE(takeSucceeded);  // Should still be false, as semaphore not given yet

  // Give the semaphore from the main thread
  EXPECT_EQ(xSemaphoreGive(sem), pdTRUE);

  // Give taker thread a chance to complete
  SimulateDelay(10);   // Let the taker thread wake up and take
  takerThread.join();  // Wait for the taker thread to finish its operation

  // Taker should now have succeeded
  EXPECT_TRUE(takeSucceeded);

  // Try taking again from main thread - should fail (it was taken by the other thread)
  EXPECT_EQ(xSemaphoreTake(sem, 0), pdFALSE);

  vSemaphoreDelete(sem);
}

// --- Test 7: Basic Task Creation and Execution ---
std::atomic<int> taskRunCount(0);  // Using atomic for thread-safe count
void simpleTaskFunction(void* parameter) {
  (void)parameter;  // Cast to void to suppress unused parameter warning
  taskRunCount++;
  std::cout << "FreeRTOS Mock: simpleTaskFunction executed. Count: " << taskRunCount.load()
            << std::endl;
  vTaskDelete(NULL);  // Task self-deletes, which is common in FreeRTOS tasks
}

TEST_F(FreeRTOSMockTest, TaskCreationExecutionDeletion) {
  std::cout << "Running Test: TaskCreationExecutionDeletion" << std::endl;
  taskRunCount = 0;  // Reset for this test

  TaskHandle_t taskHandle = nullptr;
  BaseType_t result =
      xTaskCreatePinnedToCore(simpleTaskFunction, "SimpleTask", 2048, nullptr, 1, &taskHandle, 0);

  EXPECT_EQ(result, pdTRUE);
  ASSERT_NE(taskHandle, nullptr);

  // Give the task a moment to run and self-delete
  SimulateDelay(50);  // Give enough time for the mock thread to schedule and run

  EXPECT_EQ(
      taskRunCount.load(), 1);  // Should have run once due to TaskManager's current mock behavior

  // If vTaskDelete(NULL) is called inside the task, the TaskManager mock handles cleanup.
  // No need to call vTaskDelete(taskHandle) from here.
}

// --- Test 8: Mock xTaskGetTickCount() function check ---
TEST_F(FreeRTOSMockTest, GetTickCountMockCheck) {
  std::cout << "Running Test: GetTickCountMockCheck" << std::endl;
  TickType_t start_ticks = xTaskGetTickCount();
  std::cout << "Initial ticks: " << start_ticks << std::endl;
  SimulateDelay(100);  // Simulate 100ms delay
  TickType_t end_ticks = xTaskGetTickCount();
  std::cout << "Ticks after 100ms delay: " << end_ticks << std::endl;

  // Expect ticks to have advanced by approximately 100.
  // Use EXPECT_GE/LE for range due to potential mock overhead/granularity.
  EXPECT_GE(end_ticks, start_ticks + 90);   // At least 90 ticks
  EXPECT_LE(end_ticks, start_ticks + 110);  // At most 110 ticks
}

TEST_F(FreeRTOSMockTest, AllTasksCleanupSafely) {
  std::cout << "Running Test: AllTasksCleanupSafely" << std::endl;

  std::atomic<int> ranTasks = 0;

  auto taskLambda = [](void* param) {
    std::atomic<int>* counter = static_cast<std::atomic<int>*>(param);
    (*counter)++;
    vTaskDelay(10);        // Simulate some work
    vTaskDelete(nullptr);  // Self-delete
  };

  // Create several tasks
  const int taskCount = 5;
  for (int i = 0; i < taskCount; ++i) {
    TaskHandle_t handle = nullptr;
    xTaskCreatePinnedToCore(taskLambda, "MockTask", 2048, &ranTasks, 1, &handle, 0);
  }

  // Allow tasks to execute
  SimulateDelay(100);  // Ensure all tasks run and exit

  EXPECT_EQ(ranTasks.load(), taskCount);
}
