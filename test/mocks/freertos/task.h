#pragma once

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
  std::mutex mutex;  // Protects access to the tasks map

 public:
  static TaskManager& getInstance() {
    static TaskManager instance;
    return instance;
  }

  TaskHandle_t createTask(std::function<void(void*)> func, void* param) {
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
      // garbage collection: after the task finishes, we can delete it
      std::thread([this, handle]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        this->deleteTask(handle);
      }).detach();
    });

    tasks[handle] = std::move(task);
    return handle;
  }

  void deleteTask(TaskHandle_t handle) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = tasks.find(handle);
    if (it != tasks.end()) {
      it->second->running = false;  // Signal task thread to stop
      if (it->second->thread.joinable()) {
        it->second->thread.join();  // Wait for the thread to finish execution
      }
      tasks.erase(it);  // Remove from map
    }
  }
};
