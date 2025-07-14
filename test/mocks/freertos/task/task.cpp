#include "task.h"

TaskManager& TaskManager::getInstance() {
  static TaskManager instance;
  return instance;
}

TaskHandle_t TaskManager::createTask(std::function<void(void*)> func, void* param) {
  std::lock_guard<std::mutex> lock(mutex);

  auto task = std::make_unique<Task>();
  task->function = func;
  task->parameter = param;

  TaskHandle_t handle = reinterpret_cast<TaskHandle_t>(task.get());

  task->thread = std::thread([this, handle, task = task.get()]() {
    while (task->running) {
      task->function(task->parameter);
      break;
    }

    std::thread([this, handle]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      this->deleteTask(handle);
    }).detach();
  });

  tasks[handle] = std::move(task);
  return handle;
}

void TaskManager::deleteTask(TaskHandle_t handle) {
  std::lock_guard<std::mutex> lock(mutex);

  auto it = tasks.find(handle);
  if (it != tasks.end()) {
    it->second->running = false;
    if (it->second->thread.joinable()) { it->second->thread.join(); }
    tasks.erase(it);
  }
}
