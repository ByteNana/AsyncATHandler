# ATResponse Concept

`ATResponse` is a lightweight container that accumulates parsed lines for a single AT command’s response and determines when that response is complete and whether it succeeded.

It is populated by `AsyncATHandler` via an owning `ATPromise`. Each appended `ResponseLine` records content, type, command id, and timestamp. When a final line is added, `ATResponse` marks the response as completed and sets success based on the final type.

## Data Model

- `ResponseLine` fields:
  - `content`: the raw line content as read (typically includes `\r\n`).
  - `type`: enum `ResponseType` value (see below).
  - `commandId`: id of the originating command (0 for unsolicited/URC).
  - `timestamp`: `millis()` when the line was processed.
- `ResponseType` values:
  - `FINAL_OK`: terminal success line, e.g. `OK`.
  - `FINAL_ERROR`: terminal error line, e.g. `ERROR`.
  - `FINAL_CME_ERROR`: terminal error with cause, e.g. `+CME ERROR: ...`.
  - `INTERMEDIATE_DATA`: non-final payload lines (echo, data, progress, etc.).
  - `UNSOLICITED`: URCs. These are handled by the handler’s URC callback and are not appended to a command’s `ATResponse`.

A line is considered final if `isFinalResponse()` returns `true` (OK/ERROR/CME ERROR).

## Core Behavior

- `addLine(const ResponseLine&)`: appends a line and, if the line is final, sets `completed = true` and `success = (type == FINAL_OK)`.
- `isCompleted()`: true after a final line was added.
- `isSuccess()`: true only if the final line was `OK`.
- `getId()`: id of the command this response belongs to.

## Accessors

- `getFullResponse()`: concatenates `content` of all lines in order (useful for logging or legacy parsing).
- `getDataOnly()`: concatenates only `INTERMEDIATE_DATA` lines.
- `getDataLines()`: returns a `std::vector<String>` of only `INTERMEDIATE_DATA` lines.
- `containsResponse(expected)`: substring search across all lines.

Notes
- `getFullResponse()` preserves line order and typical line endings present in `content`.
- Unsolicited lines are routed to `onURC(...)` and not included here.

## Typical Usage

Synchronous flow using the convenience API:

```cpp
String full;
bool ok = at.sendSync("AT+CSQ", full, 2000);
// When ok == true, the final line was OK; `full` holds the entire response.
```

Asynchronous flow via `ATPromise` (manual inspection):

```cpp
ATPromise* p = at.sendCommand("AT+HTTPACTION=0");
bool done = p->wait();
if (done && p->getResponse()) {
  ATResponse* r = p->getResponse();
  bool success = r->isSuccess();
  String payload = r->getDataOnly();
}
auto owned = at.popCompletedPromise(p->getId());
```

Line-oriented parsing example:

```cpp
ATPromise* p = at.sendCommand("AT+QIOPEN=1,0,\"TCP\",\"example.com\",80");
if (p->wait() && p->getResponse()) {
  for (const auto& line : p->getResponse()->getDataLines()) {
    // parse line, e.g., +QIOPEN: 0,0
  }
}
```

## Relationship to AsyncATHandler

- `AsyncATHandler` classifies lines into `ResponseType` and decides whether they belong to a promise (command) or are unsolicited (URC).
- Only lines belonging to the in-flight command are appended to its `ATResponse` via `ATPromise::addResponseLine(...)`.
- URCs are surfaced via `onURC(URCCallback)` and are not part of any `ATResponse`.

## Error Handling & Edge Cases

- `FINAL_CME_ERROR` and `FINAL_ERROR` mark the response as completed with `success = false`.
- Late data after a final line is ignored for the command’s response (the command is already complete).
- Some modules echo commands; treat echos as `INTERMEDIATE_DATA` and filter as needed using `getDataOnly()` or by matching prefixes.

