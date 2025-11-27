# ChatApp Codebase Guide for AI Agents

## Project Overview

**ChatApp** is a cross-platform multi-client chat server built in C++ (C++20) with **platform-specific I/O multiplexing**:
- **Linux**: epoll-based event handling with thread pool workers
- **Windows**: IOCP (I/O Completion Ports) for asynchronous socket operations

The architecture separates concerns into:
- `chatserver/` – Server core with platform-specific connection handlers
- `clientsocket/` – Chat client implementation
- `utils/` – Shared utilities (logging, thread pool, async helpers)

## Critical Architecture Patterns

### 1. Platform Abstraction via Virtual Methods

The design uses **abstract base class** `ChatServer` with platform-specific subclasses:

```cpp
// chatserver/chatserver.h
class ChatServer {
    virtual void acceptConnections() = 0;  // Implemented in HandleConnectionsLinux/Windows
};

// Linux: event-driven with epoll + thread pool
class HandleConnectionsLinux : public ChatServer { ... };

// Windows: async I/O with IOCP
class HandleConnectionsWindows : public ChatServer { ... };
```

**When modifying networking code**: Edit the platform-specific class (`handleConnectionsLinux.cpp` or `handleConnectionsWindows.cpp`), NOT the base class. The base class only defines common socket management (`createListner()`, `closeSocket()`, `getClientSockets()`).

### 2. Thread-Safe Message Broadcasting

Messages flow through a **decoupled queue pattern**:

```cpp
// chatserver.h (base class)
queue<pair<int, string>> m_BroadcastMessageQueue;  // int=socket descriptor, string=message
void threadBroadcastMessage();  // Runs in dedicated jthread

// Pattern: Client reads → enqueue to broadcast queue → broadcast thread sends to all
```

**Critical detail**: When a client message arrives, it's enqueued with the recipient socket ID, NOT immediately broadcasted. This prevents blocking on individual socket writes. The `threadBroadcastMessage()` thread processes the queue asynchronously.

### 3. Logger: Async File Writing with Rotation

`utils/logger.h` implements **asynchronous logging** via internal thread:

```cpp
class Logger {
    queue<string> m_queue;
    jthread m_ThreadProcessMessages;  // Background thread processing queue
    
    void processingMessages();  // Called automatically on construction
};

// Usage
logger.log(LogLevel::Info, "message: {}", variable);  // Non-blocking enqueue
logger.setDone(true);  // Signal shutdown; destructor waits for queue drain
```

**Key pattern**: All logging calls are non-blocking. The destructor calls `stopProcessing()` to gracefully flush the queue before logger destruction. Always call `setDone(true)` before shutting down servers.

### 4. Singleton Factories for Resource Management

Resources use **static singleton factories** to ensure single instantiation:

```cpp
// Logger factory
Logger& logger = LoggerFactory::getInstance("server.log", MAX_LOG_FILE_SIZE);

// ClientSocket factory
ClientSocket& client = ClientSocketFactory::getInstance(ip, port, "client.log");
```

**Do not manually instantiate** Logger or ClientSocket; always use factories. This prevents multiple log file handles or socket instances.

## Build & Platform Specifics

### CMake Build System

```bash
# Debug build (includes Linux epoll/Windows IOCP)
cmake -B build-debug
cmake --build build-debug

# Release build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

**Windows vs Linux differentiation** (CMakeLists.txt):
```cmake
if(WIN32)
    list(APPEND SOURCES handleConnectionsWindows.cpp)
    target_link_libraries(chatServer ws2_32)  # Winsock2
else()
    find_package(fmt CONFIG REQUIRED)  # Linux uses fmt for string formatting
    list(APPEND SOURCES handleConnectionsLinux.cpp ../utils/threadPool.cpp)
    target_link_libraries(chatServer pthread fmt::fmt)
endif()
```

**String formatting difference**:
- **Windows**: Uses `std::format` (C++20)
- **Linux**: Uses `<fmt/core.h>` external library

Abstracting this in `logger.h`:
```cpp
#ifdef _WIN32
    #include <format>
    template<typename... Args>
    inline string formatString(...) { return vformat(...); }
#else
    #include <fmt/core.h>
    template<typename... Args>
    inline string formatString(fmt::format_string<Args...> fmt, ...) { return fmt::format(...); }
#endif
```

## Data Structures & Concurrency

### Thread-Safe Queue (utils/threadSafeQueue.h)

Generic queue with blocking/non-blocking pop:

```cpp
template<typename T>
class threadSafeQueue {
    void waitAndPop(T& value);  // Blocks until data available
    bool tryPop(T& value);      // Non-blocking, returns false if empty
};
```

Used for:
- Function wrapper queue in thread pool (`threadPool_` class)
- Message broadcasting queue in server

### Function Wrapper (utils/functionWrapper.h)

Allows storing type-erased callable objects (lambdas, functions) in a queue:

```cpp
// In threadPool_::submit()
m_WorkQueue.push(move(task));  // Pushes packaged_task wrapped in functionWrapper
```

**When adding tasks**: Use `threadPool->enqueue()` (simple lambda) or `threadPool_->submit()` (with return value).

## Key Entry Points

| Component | Main Entry | Init Pattern |
|-----------|-----------|--------------|
| **Server** | `chatserver/main.cpp` | Parses args → `StartServer` → `ChatServer::createListner()` → `acceptConnections()` loop |
| **Client** | `clientsocket/main.cpp` | Connects → spawns send/read threads → runs until "quit" received |
| **Logger** | `LoggerFactory::getInstance()` | Lazy-initialized static instance, auto-starts background processing thread |

## Platform-Specific Implementation Notes

### Linux (handleConnectionsLinux.cpp)

- Uses **epoll** for efficient event multiplexing
- Thread pool with `N = hardware_concurrency()` workers
- Non-blocking sockets with edge-triggered mode (`EPOLLET`)
- Each client event triggers `threadPool->enqueue([clientFd, this] { handleClient(clientFd); })`

```cpp
// Key constants
constexpr int MAX_QUEUE_CONNECTINON{10};  // epoll_wait batch size
constexpr int BUFFER_SIZE{1024};          // Socket read buffer
epoll_event m_Events[MAX_QUEUE_CONNECTINON];  // Event array
```

### Windows (handleConnectionsWindows.cpp)

- Uses **IOCP** for native async socket handling
- Worker threads associated with IOCP handle completions
- Async reads/writes don't block the accept loop

## Socket Management Conventions

```cpp
// All socket types are platform-agnostic in base class:
#ifdef _WIN32
    SOCKET m_SockfdListener;  // Windows: unsigned long long
#else
    int m_SockfdListener;     // Linux: file descriptor (int)
#endif

// Common operations across platforms:
bool createListner();               // Bind & listen
void closeSocket(int sd);           // Close single client socket
void closeAllClientSockets();       // Clean shutdown
unordered_set<int> getClientSockets() const;  // Connected clients
bool getClientIP(int sd);           // Log client info
```

## Logging Best Practices

```cpp
// Always use lazy formatting (no runtime cost if log level suppressed in future)
m_Logger.log(LogLevel::Info, "Client connected: {} {}", ip, port);

// Log levels: Debug, Info, Warning, Error
// Messages auto-timestamp and level-prefix in background thread

// Always call before shutdown:
m_Logger.setDone(true);  // Signals background thread to finish after queue drain
```

## Testing & Debugging

- **No automated tests** currently (test structure prepared in CMakeLists.txt with `include(CTest)`)
- Manual testing: Start server on one terminal, connect client(s) in others
- Log files generated in `log/` directory with timestamps on rotation
- Debug symbols included via `-g` compiler flag in CMake

## Dependencies

- **CMake**: 3.10+
- **C++20** compiler (g++, MSVC)
- **fmt library** (Linux only): `find_package(fmt CONFIG REQUIRED)`
- **Winsock2** (Windows): auto-linked
- **pthreads** (Linux): auto-linked

## Common Modification Points

1. **Adding new message type**: Extend `handleClient()` logic in platform-specific handler
2. **Changing broadcast strategy**: Modify `threadBroadcastMessage()` in `chatserver.cpp`
3. **Tuning concurrency**: Adjust `MAX_THREAD`, `MAX_QUEUE_CONNECTINON` constants in `chatserver.h`
4. **Cross-platform code**: Use `#ifdef _WIN32` guards; update both platform handlers
5. **Logger changes**: Ensure `setDone(true)` is called before app exit to flush async queue
