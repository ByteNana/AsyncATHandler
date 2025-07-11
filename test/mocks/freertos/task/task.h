#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

typedef void* TaskHandle_t;

class TaskManager {
 private:
  struct Task {
    std::thread thread;
    std::atomic<bool> running{true};
    std::function<void(void*)> function;
    void* parameter;
  };

  std::map<TaskHandle_t, std::unique_ptr<Task>> tasks;
  std::mutex mutex;

 public:
  static TaskManager& getInstance();
  TaskHandle_t createTask(std::function<void(void*)> func, void* param);
  void deleteTask(TaskHandle_t handle);
};
