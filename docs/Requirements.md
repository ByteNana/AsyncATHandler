# AsyncATHandler Requirements and Design Document

## 1. Overview

The AsyncATHandler is a FreeRTOS-based library designed to manage asynchronous AT command communication with cellular modems, GSM modules, and other devices that use the Hayes AT command protocol. The library provides both synchronous and asynchronous command interfaces while handling unsolicited responses through a callback mechanism.

## 2. System Requirements

### 2.1 Functional Requirements

#### FR1: Command Management
- **FR1.1**: Support asynchronous (fire-and-forget) AT command sending
- **FR1.2**: Support synchronous AT command sending with response collection
- **FR1.3**: Support batch command execution with individual response tracking
- **FR1.4**: Support variadic template command building from multiple string parts
- **FR1.5**: Support command queuing with configurable queue size
- **FR1.6**: Support command timeout handling with configurable timeouts

#### FR2: Response Processing
- **FR2.1**: Process incoming serial data line-by-line (CR+LF terminated)
- **FR2.2**: Classify responses as OK, ERROR, data, or unsolicited
- **FR2.3**: Route unsolicited responses to registered callback functions
- **FR2.4**: Aggregate multi-line responses for synchronous commands
- **FR2.5**: Support expected response validation beyond standard OK/ERROR
- **FR2.6**: Handle response timeouts gracefully

#### FR3: Concurrency and Thread Safety
- **FR3.1**: Use FreeRTOS tasks for background response processing
- **FR3.2**: Provide thread-safe access to all public methods
- **FR3.3**: Support multiple concurrent command requests
- **FR3.4**: Use semaphores for synchronous command synchronization
- **FR3.5**: Use mutexes for critical section protection

#### FR4: Resource Management
- **FR4.1**: Manage FreeRTOS resources (tasks, queues, semaphores, mutexes)
- **FR4.2**: Provide clean initialization and cleanup mechanisms
- **FR4.3**: Handle resource allocation failures gracefully
- **FR4.4**: Prevent resource leaks and double-initialization

### 2.2 Non-Functional Requirements

#### NFR1: Performance
- **NFR1.1**: Minimize response latency for synchronous commands
- **NFR1.2**: Support configurable queue sizes for scalability
- **NFR1.3**: Efficient memory usage with fixed-size buffers
- **NFR1.4**: Low CPU overhead during idle periods

#### NFR2: Reliability
- **NFR2.1**: Handle buffer overflows gracefully without crashing
- **NFR2.2**: Recover from communication errors automatically
- **NFR2.3**: Provide comprehensive error logging
- **NFR2.4**: Maintain operation during partial system failures

#### NFR3: Maintainability
- **NFR3.1**: Modular architecture with clear separation of concerns
- **NFR3.2**: Configurable parameters through header file constants
- **NFR3.3**: Comprehensive logging for debugging and monitoring
- **NFR3.4**: Clear and consistent API design

## 3. Architecture Design

### 3.1 Component Architecture

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Application   │    │   Test Suite    │    │   User Code     │
└─────────────────┘    └─────────────────┘    └─────────────────┘
         │                       │                       │
         └───────────────────────┼───────────────────────┘
                                 │
                    ┌─────────────────────────┐
                    │   AsyncATHandler API    │
                    │  (Public Interface)     │
                    └─────────────────────────┘
                                 │
         ┌───────────────────────┼───────────────────────┐
         │                       │                       │
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│  Core Module    │    │ Commands Module │    │ Responses Module│
│ (Lifecycle)     │    │ (Send/Queue)    │    │ (Process/Route) │
└─────────────────┘    └─────────────────┘    └─────────────────┘
         │                       │                       │
         └───────────────────────┼───────────────────────┘
                                 │
                    ┌─────────────────────────┐
                    │    Utils Module         │
                    │ (Helpers/Callbacks)     │
                    └─────────────────────────┘
                                 │
                    ┌─────────────────────────┐
                    │    Tasks Module         │
                    │  (Background Worker)    │
                    └─────────────────────────┘
                                 │
                    ┌─────────────────────────┐
                    │   FreeRTOS & Hardware   │
                    │    (Serial Stream)      │
                    └─────────────────────────┘
```

### 3.2 Module Responsibilities

#### 3.2.1 Core Module (`AsyncATHandler.core.cpp`)
- **Purpose**: Lifecycle management and resource initialization
- **Responsibilities**:
  - Constructor/destructor implementation
  - FreeRTOS resource creation (queues, semaphores, mutexes)
  - Task creation and management
  - Clean shutdown and resource cleanup
- **Key Methods**: `AsyncATHandler()`, `~AsyncATHandler()`, `begin()`, `end()`

#### 3.2.2 Commands Module (`AsyncATHandler.commands.cpp`)
- **Purpose**: Command sending and synchronous response handling
- **Responsibilities**:
  - Asynchronous command queuing
  - Synchronous command execution with response collection
  - Batch command processing
  - Command ID generation and tracking
- **Key Methods**: `sendCommandAsync()`, `sendCommand()`, `sendCommandBatch()`

#### 3.2.3 Responses Module (`AsyncATHandler.responses.cpp`)
- **Purpose**: Response processing and routing
- **Responsibilities**:
  - Serial data buffering and line processing
  - Response classification and routing
  - Queue management for responses
  - Pending command state management
- **Key Methods**: `processIncomingData()`, `handleResponse()`, `flushResponseQueue()`

#### 3.2.4 Utils Module (`AsyncATHandler.utils.cpp`)
- **Purpose**: Utility functions and callback management
- **Responsibilities**:
  - Unsolicited response detection
  - Callback registration and management
  - Queue introspection methods
  - Helper functions
- **Key Methods**: `isUnsolicitedResponse()`, `setUnsolicitedCallback()`, `getQueuedCommandCount()`

#### 3.2.5 Tasks Module (`AsyncATHandler.tasks.cpp`)
- **Purpose**: Background task implementation
- **Responsibilities**:
  - Command processing from queue
  - Serial communication with hardware
  - Continuous response monitoring
  - Task lifecycle management
- **Key Methods**: `readerTaskFunction()`

## 4. Data Flow and Interactions

### 4.1 Asynchronous Command Flow

```
[User Code] → sendCommandAsync() → [Command Queue] → [Reader Task] → [Serial Stream]
```

1. User calls `sendCommandAsync(command)`
2. Command module creates `ATCommand` struct with unique ID
3. Command is queued in `commandQueue` with `waitForResponse = false`
4. Reader task dequeues command and sends to serial stream
5. Method returns immediately (fire-and-forget)

### 4.2 Synchronous Command Flow

```
[User Code] → sendCommand() → [Command Queue] → [Reader Task] → [Serial Stream]
                    ↓                                                    ↓
              [Wait on Semaphore] ← [Response Processing] ← [Incoming Data]
                    ↓
              [Collect Responses] → [Return Result]
```

1. User calls `sendCommand(command, response, expected, timeout)`
2. Command module creates binary semaphore and `ATCommand` struct
3. Command queued with `waitForResponse = true`
4. Method blocks on semaphore with specified timeout
5. Reader task sends command and sets pending command state
6. Response module processes incoming data and routes to response queue
7. When final response (OK/ERROR) received, semaphore is signaled
8. Command module collects all responses for command ID
9. Method returns with aggregated response and success status

### 4.3 Unsolicited Response Flow

```
[Serial Stream] → [Reader Task] → [Response Processing] → [Callback Function] → [User Code]
```

1. Unsolicited data arrives on serial stream
2. Response module buffers and processes line-by-line
3. `isUnsolicitedResponse()` identifies unsolicited responses
4. Response is routed to registered callback function
5. User code handles unsolicited response asynchronously

### 4.4 Response Classification Logic

```
Incoming Response Line
         ↓
    [Trim & Validate]
         ↓
    [isUnsolicitedResponse()?] → YES → [Route to Callback] → [Return]
         ↓ NO
    [Pending Sync Command?] → NO → [Queue as Unmatched] → [Return]
         ↓ YES
    [Classify Response Type]
         ↓
    ┌─────────┬─────────┬─────────┐
    │   OK    │  ERROR  │  DATA   │
    └─────────┴─────────┴─────────┘
         ↓         ↓         ↓
    [Success]  [Failure]  [Continue]
         ↓         ↓         ↓
    [Signal]   [Signal]   [Queue]
         ↓         ↓         ↓
    [Clear]    [Clear]    [Wait]
```

## 5. Configuration Requirements

### 5.1 Queue Configuration
```cpp
#define AT_COMMAND_QUEUE_SIZE 10      // Number of commands that can be queued
#define AT_RESPONSE_QUEUE_SIZE 20     // Number of responses that can be buffered
```

### 5.2 Task Configuration
```cpp
#define AT_TASK_STACK_SIZE configMINIMAL_STACK_SIZE * 4  // Reader task stack
#define AT_TASK_PRIORITY tskIDLE_PRIORITY + 2            // Reader task priority
#define AT_TASK_CORE 1                                   // CPU core assignment
```

### 5.3 Timing Configuration
```cpp
#define AT_DEFAULT_TIMEOUT 5000       // Default command timeout (ms)
#define AT_QUEUE_TIMEOUT 100          // Queue operation timeout (ms)
```

### 5.4 Buffer Configuration
```cpp
#define AT_RESPONSE_BUFFER_SIZE 1024           // Line buffer size
#define AT_COMMAND_MAX_LENGTH 512              // Maximum command length
#define AT_EXPECTED_RESPONSE_MAX_LENGTH 512    // Maximum expected response length
```

## 6. Error Handling Requirements

### 6.1 Resource Allocation Failures
- **Requirement**: All resource allocation failures must be detected and handled
- **Implementation**: Check return values of FreeRTOS creation functions
- **Recovery**: Clean up partial allocations and return error status

### 6.2 Queue Full Conditions
- **Requirement**: Handle queue full conditions gracefully
- **Implementation**: Use configurable timeouts for queue operations
- **Recovery**: Return error status to caller, log warning messages

### 6.3 Communication Timeouts
- **Requirement**: Support configurable timeouts for all operations
- **Implementation**: Use FreeRTOS tick-based timeouts
- **Recovery**: Return timeout status, clean up resources

### 6.4 Buffer Overflows
- **Requirement**: Prevent buffer overflows from causing system instability
- **Implementation**: Check buffer bounds before writing
- **Recovery**: Clear buffer and continue processing, log warning

## 7. Thread Safety Requirements

### 7.1 Mutual Exclusion
- **Requirement**: Protect all shared data structures with mutexes
- **Implementation**: Single mutex for all critical sections
- **Scope**: Stream access, queue operations, buffer access, callback management

### 7.2 Synchronization
- **Requirement**: Use binary semaphores for command/response synchronization
- **Implementation**: One semaphore per synchronous command
- **Lifecycle**: Created before command send, deleted after response

### 7.3 Atomic Operations
- **Requirement**: Use atomic operations for simple shared variables
- **Implementation**: Command ID generation, state flags
- **Benefit**: Reduce mutex contention for simple operations

## 8. Testing Requirements

### 8.1 Unit Testing
- **Requirement**: Each module must be independently testable
- **Implementation**: Modular architecture with clear interfaces
- **Coverage**: Core functionality, error conditions, edge cases

### 8.2 Integration Testing
- **Requirement**: Test complete command/response cycles
- **Implementation**: Mock stream implementation for controlled testing
- **Scenarios**: Success cases, timeout cases, error responses

### 8.3 Concurrency Testing
- **Requirement**: Test thread safety and concurrent operations
- **Implementation**: Multi-task test scenarios
- **Focus**: Race conditions, deadlocks, resource leaks

### 8.4 Performance Testing
- **Requirement**: Validate performance under load
- **Implementation**: Batch commands, continuous operation
- **Metrics**: Response latency, memory usage, CPU utilization

## 9. API Design Requirements

### 9.1 Consistency
- **Requirement**: Consistent parameter ordering and naming
- **Implementation**: Command first, response second, options last
- **Example**: `sendCommand(command, response, expected, timeout)`

### 9.2 Flexibility
- **Requirement**: Support multiple usage patterns
- **Implementation**: Method overloads and default parameters
- **Variants**: With/without response capture, with/without expected response

### 9.3 Error Reporting
- **Requirement**: Clear error indication and logging
- **Implementation**: Boolean return values, comprehensive logging
- **Levels**: Error, warning, debug, info

### 9.4 Resource Management
- **Requirement**: RAII principles for automatic resource management
- **Implementation**: Constructor/destructor resource lifecycle
- **Guarantee**: No resource leaks under normal operation

## 10. Extensibility Requirements

### 10.1 Protocol Support
- **Requirement**: Easy addition of new unsolicited response types
- **Implementation**: Pattern-based detection in `isUnsolicitedResponse()`
- **Method**: Add new patterns to detection function

### 10.2 Configuration
- **Requirement**: Runtime and compile-time configuration options
- **Implementation**: Header-based defines with fallback defaults
- **Scope**: Queue sizes, timeouts, buffer sizes, task parameters

### 10.3 Callback System
- **Requirement**: Extensible callback mechanism
- **Implementation**: `std::function` based callbacks
- **Future**: Support multiple callback registration, filtering

### 10.4 Hardware Abstraction
- **Requirement**: Support different hardware interfaces
- **Implementation**: Stream interface abstraction
- **Compatibility**: Serial, SoftwareSerial, custom streams

This requirements document serves as the foundation for the current AsyncATHandler implementation and guides future development and maintenance activities.
