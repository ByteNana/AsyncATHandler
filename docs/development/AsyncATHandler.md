# AsyncATHandler Concept

`AsyncATHandler` is the coordinator that:
- Writes AT commands to a `Stream`.
- Spawns a reader task to parse incoming bytes into lines.
- Classifies lines (final/unsolicited/intermediate) and routes them.
- Delivers lines to the correct `ATPromise` (per-command response) or to a URC callback.

See also: [ATPromise](./ATPromise.md), [ATResponse](./ATResponse.md).

## Lifecycle

- `bool begin(Stream& stream)`: Initializes internal mutexes and starts a FreeRTOS reader task (stack 4096, prio 2, pinned to core 1). Must be called before sending.
- `void end()`: Stops the reader task, clears pending promises, and releases resources. Safe to call multiple times; destructor calls `end()`.
- `Stream* getStream()`: Returns the active stream or `nullptr`.

Requirements
- A duplex `Stream` (e.g., Arduino `Serial` for ESP32) that supports `print`, `read`, `available`, and `flush`.

## Sending Commands

- `ATPromise* sendCommand(const String& command)`: Enqueues a new promise, prints `command` with `\r\n`, returns a pointer to the in-flight promise.
- `template <typename... Args> ATPromise* sendCommand(Args... parts)`: Convenience concatenation of parts into a single command.
- `bool sendSync(const String& command, String& response, uint32_t timeout = 5000)`: Synchronous helper that waits on the underlying promise, fills `response` with the full response text, and returns success (`OK`).
- `bool sendSync(const String& command, uint32_t timeout = 5000)`: Overload that discards text, returns success only.

Ownership & Cleanup
- Internally, the handler stores promises in `std::vector<std::unique_ptr<ATPromise>>`.
- `sendSync(...)` pops the completed promise automatically.
- For manual async usage, call `popCompletedPromise(promise->getId())` when done to take ownership and let it destruct out of scope.

## Unsolicited Responses (URCs)

- `void onURC(URCCallback cb)`: Registers a callback invoked for unsolicited lines (e.g., `+QIURC`, `+CMTI`, etc.).
- URCs are not appended to any command response; they are surfaced directly via the callback.

## Parsing Pipeline

Reader Task
- Continuously reads bytes from the `Stream`, appends to an internal `lineBuffer`, and detects line completion.
- Completion rules (`isLineComplete`):
  - Lines ending with `\r\n` are complete.
  - A single prompt line starting with `>` is trimmed and treated as complete with `\r\n` appended.
  - The buffer is capped (512 chars) to avoid runaway; overflow clears the buffer and logs a warning.

Classification (`classifyLine`)
- Final: `OK`, `ERROR`, `+CME ERROR:` → routed to an in-flight promise and marks its `ATResponse` as completed.
- Unsolicited: heuristic prefixes such as `+CMT:`, `+CMTI:`, `+CLIP:`, `+CREG:`, `+CGREG:`, `+CEREG:`, `+QIURC:`, `+QMTRECV:`, `+QIOPEN:`, `+QIRD:`, `+QMTSTAT:`, `+QSSLOPEN:`, `+QSSLURC:`, `+QSSLRECV:`, `+QICLOSE` → dispatched to `onURC`.
- Intermediate: everything else → appended to the appropriate in-flight promise as data.

Routing to Promises (`findPromiseForResponse`)
- First tries to find an in-flight promise whose next expectation matches the line (substring check).
- If none matches, falls back to the oldest incomplete promise.
- Appends as a `ResponseLine` with timestamp; `ATPromise` decides when to signal completion (final line or satisfied expectations).

## Concurrency & Synchronization

- Uses two FreeRTOS mutexes:
  - A general mutex guarding multi-step sequences around send/wait.
  - A data mutex for safe access to the pending promises vector.
- The reader task yields with `vTaskDelay(10ms)` between polls.
- Logging uses ESP-IDF-style macros (e.g., `log_i`, `log_d`) governed by `LOG_LEVEL`.

## Examples

Synchronous command (typical usage):

```cpp
String resp;
bool ok = at.sendSync("AT", resp, 1000);
if (ok) {
  // Command succeeded; `resp` contains full response text (incl. CRLFs)
}
```

Asynchronous with expectations and URC handling:

```cpp
at.onURC([](const String& urc){
  // handle network registration changes, socket events, etc.
});

ATPromise* p = at.sendCommand("AT+QIOPEN=1,0,\"TCP\",\"example.com\",80");
p->expect("+QIOPEN:").timeout(10000);
if (p->wait() && p->getResponse()) {
  bool success = p->getResponse()->isSuccess();
}
auto owned = at.popCompletedPromise(p->getId());
```

## Tips & Edge Cases

- Some modules echo commands; treat echo lines as intermediate data.
- A timeout in `sendSync`/`wait` does not cancel the device; late lines may still arrive and be routed.
- Choose expectations to disambiguate concurrent commands and ensure correct routing.
- If you interleave many commands rapidly, consider backpressure at the AT layer or higher-level sequencing.

