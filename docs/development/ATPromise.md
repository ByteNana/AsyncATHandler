# ATPromise Concept

`ATPromise` represents the in-flight result of a single AT command sent by `AsyncATHandler`. It accumulates the command’s response lines, allows you to declare expected substrings that must appear (in order), and provides a `wait()` method to block until completion or timeout.

Typical flow:

1) `AsyncATHandler::sendCommand(...)` creates and returns an `ATPromise*`.
2) Optionally chain `.expect(...)` calls and/or `.timeout(ms)`.
3) Call `wait()` to block for completion, then inspect `getResponse()`.

> For most use-cases, prefer the convenience API `sendSync(...)`, which internally uses an `ATPromise` and returns when complete.

## Lifecycle & Ownership

- Creation: Only `AsyncATHandler` constructs `ATPromise` instances when you call `sendCommand(...)`.
- Storage: The handler keeps ownership in an internal `std::vector<std::unique_ptr<ATPromise>>` until the promise is popped.
- Completion: The promise transitions to completed when either:
  - A final response line is received (`OK`, `ERROR`, or `+CME ERROR:`), or
  - All declared expectations are satisfied (see Expectations below).
- Cleanup: If you called `sendSync(...)`, the handler pops and destroys the promise automatically. For manual async usage, call `handler.popCompletedPromise(promise->getId())` after you’re done to take ownership and destroy it when it goes out of scope.

## Core API (overview)

- `ATPromise* expect(const String& expected)`: enqueue an expected substring (FIFO) that must appear in order in subsequent response lines. Returns `this` to allow chaining.
- `ATPromise* timeout(uint32_t ms)`: set the wait timeout for `wait()`.
- `bool wait()`: blocks up to the configured timeout. Returns `true` if completion was signaled, `false` on timeout. Completion does not imply success — inspect the response to know whether the device returned `OK`.
- `ATResponse* getResponse()`: access the accumulated response.
- `uint32_t getId() const`: unique command id, useful for `popCompletedPromise(...)`.

Internal methods (used by the handler):

- `void addResponseLine(const ResponseLine& line)`: appends a parsed line to the response, checks expectations, and signals completion when conditions are met.
- `bool matchesExpected(const String& line) const`: returns `true` if the next expected substring is contained in `line`.
- `bool isCompleted() const`: indicates whether a final response has been observed or expectations were satisfied.

## Expectations

Expectations are optional and help disambiguate which in-flight command a particular line belongs to when several are outstanding.

- Ordering: expectations are matched in the order they were added. Each match pops the front of the queue.
- Matching: substring match (`String::indexOf(...) != -1`), not a full-line match.
- Completion-by-expectations: if expectations are present and the queue becomes empty, the promise is considered complete and `wait()` will unblock (even if a final `OK/ERROR` has not yet arrived).
- No expectations: completion relies solely on the receipt of a final response (`OK`, `ERROR`, `+CME ERROR:`).

## Completion and Success

- Completion means the promise has enough information to stop waiting.
- Success is determined from the final response line recorded by `ATResponse`:
  - `OK` → success
  - `ERROR` or `+CME ERROR:` → failure
- Inspect via `promise->getResponse()->isSuccess()` and retrieve payload with `getFullResponse()`, `getDataOnly()`, or `getDataLines()`.

## Threading Model

- `wait()` uses a FreeRTOS binary semaphore internally.
- On timeout or completion, the semaphore is given back to allow a subsequent `wait()` if needed (e.g., polling-style usage). In practice, prefer a single `wait()` call per promise to avoid ambiguous timing.

## Usage Examples

Synchronous (recommended for most cases):

```cpp
String full;
bool ok = at.sendSync("AT+CSQ", full, 2000);
if (ok) {
  // Parse RSSI/BER from `full` or use ATResponse helpers in a custom flow.
}
```

Asynchronous with expectations and manual cleanup:

```cpp
ATPromise* p = at.sendCommand("AT+QIOPEN=1,0,\"TCP\",\"example.com\",80,0,1");
p->expect("+QIOPEN:").timeout(10000);  // Expect URC-like open result
bool completed = p->wait();
if (completed && p->getResponse()) {
  bool success = p->getResponse()->isSuccess();
  String full = p->getResponse()->getFullResponse();
  // ...inspect `full` and `success`...
}

// Take ownership and let it destruct when leaving scope
auto owned = at.popCompletedPromise(p->getId());
```

Notes:

- URCs are handled separately via `AsyncATHandler::onURC(URCCallback)`. They are not associated with a specific promise unless you model them with expectations and routing logic.
- A timeout in `wait()` returns `false` but the device may still send a late response afterwards; handle such late data according to your state machine.
- Use expectations to steer lines to the correct promise when you have multiple concurrent commands.

