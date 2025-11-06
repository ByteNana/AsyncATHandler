# AsyncSmartLock Concept

`AsyncSmartLock` is a lightweight RAII-style wrapper around a FreeRTOS mutex.
It provides a clean and exception-safe way to synchronize access to shared resources across asynchronous tasks.

Internally, it uses a `SemaphoreHandle_t` created with `xSemaphoreCreateMutex`, and exposes a smart `guard()` helper that automatically releases the lock when it goes out of scope.

---

## Lifecycle

* The constructor allocates a FreeRTOS mutex and asserts its validity.
* The destructor deletes the mutex.
* Both are safe and deterministic — no heap allocations beyond the FreeRTOS API itself.

---

## Locking

### `void lock()`

Acquires the mutex, blocking indefinitely (`portMAX_DELAY`) until it becomes available.

### `void unlock()`

Releases the mutex.
Must only be called by the task that currently holds the lock.

### `Ptr guard()`

Convenience helper that returns a scoped RAII guard (`std::shared_ptr<void>`).
When the guard object goes out of scope, its custom deleter automatically calls `unlock()` on the original lock.

This allows for safe and automatic unlocking even when functions return early or throw exceptions.

---

## Usage Example

```cpp
AsyncSmartLock lock;

void updateSharedState() {
  auto g = lock.guard(); // Automatically acquires the lock
  // ... perform critical section ...
} // Automatically released here
```

No manual unlocking required — the `guard` object takes care of releasing the mutex as soon as it’s destroyed.

---

## Notes

* Built on top of **FreeRTOS mutexes**, ensuring full task-level safety.
* Mimics `std::lock_guard` semantics for embedded contexts.
* Eliminates common pitfalls with manual `xSemaphoreTake` / `xSemaphoreGive` patterns.
* Lightweight: no dynamic memory beyond FreeRTOS internal allocations.


