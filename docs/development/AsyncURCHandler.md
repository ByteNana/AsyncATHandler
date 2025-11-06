Perfect — you want your `AsyncURCHandler` doc to match that style and level of completeness.
Here’s a version that fits seamlessly alongside your `AsyncATHandler`, written in the same tone, structure, and formatting:

---

# AsyncURCHandler Concept

`AsyncURCHandler` manages unsolicited result code (URC) callbacks for `AsyncATHandler`.
URCs are lines sent by the modem that are **not part of any command’s response** (e.g. network status notifications, incoming data, socket events).
The handler lets you **register**, **update**, and **remove** callbacks for URCs based on a string prefix, and automatically invokes them when matching lines arrive.

---

## Lifecycle

* Created and owned by `AsyncATHandler`; users do not instantiate it directly.
* Lives for the entire lifetime of the parent handler.
* Automatically used by the reader task to dispatch unsolicited lines.

---

## Core API (overview)

### Registration

* `void registerEvent(const String& pattern, URCCallback cb)`
  Registers a new URC pattern and its callback, or updates the callback if the pattern already exists.

  * Ignores empty patterns or null callbacks (logs a warning).
  * Matching is **prefix-based** (`line.startsWith(pattern)`).
  * Typical patterns: `"+CMTI:"`, `"+QIURC:"`, `"+QIOPEN:"`.

* `void unregisterEvent(const String& pattern)`
  Removes a previously registered callback for the given pattern.
  Logs a warning if no handler was found.

### Query Utilities

* `bool isPattern(const String& pattern)`
  Returns `true` if a callback is registered for the given pattern.

* `bool isMatch(const String& line)`
  Returns `true` if the given line matches the start of any registered pattern.

### Dispatching

* `void handleUnsolicitedResponse(const String& line)`
  Called internally by `AsyncATHandler` when a received line does not belong to any in-flight `ATPromise`.

  * Scans all registered patterns and collects those whose prefix matches the line.
  * Copies the matching callbacks into a temporary list under a lock.
  * Invokes them **outside the lock** to prevent deadlocks.
  * Supports multiple callbacks if multiple patterns match.

---

## Matching Rules

* **Prefix-based:** `line.startsWith(pattern)` determines matches.
* **Case-sensitive:** Pattern and incoming line must match exactly in case.
* **Multiple matches:** If several patterns match, *all* associated callbacks are executed.
* **Null callbacks:** Handlers with a `nullptr` callback are ignored during dispatch.

---

## Threading & Synchronization

* All access to the internal handler list is guarded by a lightweight mutex (`lock.guard()`), ensuring thread-safe registration and lookup.
* Callbacks are always invoked **after releasing the lock**, allowing them to safely call back into `AsyncATHandler` if needed.
* No heap allocations occur during dispatch aside from the temporary callback list.

---

## Logging Behavior

Uses ESP-IDF logging macros:

* `log_v`: verbose — registration, updates, removals
* `log_w`: warnings — invalid registration or missing handlers
  This keeps the runtime overhead minimal while still traceable when verbose logging is enabled.

---

## Example Usage

```cpp
AsyncATHandler at;

// Register a callback for incoming SMS URCs
at.onURCHandler().registerEvent("+CMTI:", [](const String& urc) {
  ESP_LOGI("URC", "Received SMS indication: %s", urc.c_str());
});

// Register a socket event handler
at.onURCHandler().registerEvent("+QIURC:", [](const String& urc) {
  ESP_LOGI("URC", "Socket event: %s", urc.c_str());
});

// Later, when a line "+CMTI: "SM",3" arrives,
// AsyncURCHandler automatically calls the matching callback.
```

---

## Internal Implementation Notes

* **Storage:** Maintains a `std::vector<URCHandler>` where each element holds `{ pattern, URCCallback }`.
* **Helpers:**

  * `findPattern(pattern)` → exact match lookup (used by register/unregister).
  * `findMatch(line)` → first prefix match (used internally).
* **Lock scope:** Registration and pattern searches occur under the lock; callback invocation happens afterward.
* **Extensibility:** Designed to be lightweight and easily extended to pattern-matching strategies beyond simple prefix checks.

---

## Tips & Edge Cases

* Register events *before* calling `AsyncATHandler::begin()` to ensure callbacks are ready when the reader starts.
* Avoid long-running work in callbacks; offload heavy logic to a queue or task.
* Overlapping patterns (e.g., `"+Q"` and `"+QIOPEN:"`) will both trigger if they match — keep patterns as specific as needed.
* Unregister handlers when no longer needed to avoid unnecessary dispatches.

