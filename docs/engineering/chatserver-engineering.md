# Chat Server Engineering Guide

## Scope

This guide documents implementation-level details of the C++20 server in `chatserver`: modules, interfaces, configuration, build/deployment commands, coding patterns, error/logging behavior, and test status. For the system topology and design constraints, see [the architecture guide](../architecture/chatserver-architecture.md).

The server directory contains 12 files: `main.cpp`, `startserver.h/.cpp`, `chatServerFactory.h`, `chatserver.h/.cpp`, `handleConnectionsLinux.h/.cpp`, `handleConnectionsWindows.h/.cpp`, `CMakeLists.txt`, and `CMakePresets.json`. The implementation also depends on `utils` and interoperates with the repository's `clientsocket`.

## Module-by-module breakdown

### `main.cpp`

**Role:** Process entry point and positional CLI parsing.

- Defaults: host `localhost`, port `8080`, log file `chatServer.log`.
- `usage(...)` prints argument help for `-h` and `--help`.
- Constructs `StartServer(logFileName, serverName, portNumber)` and invokes `Run()`.
- First positional argument is treated as a server name unless parsing it as an integer indicates otherwise. Second positional argument is parsed as a port; values outside 1–65535 or parse failures fall back to the default. Third argument is the log filename.

The current parser is permissive and its integer checks do not validate that the entire input token was consumed. It also uses `stoi`, which can accept a numeric prefix followed by other characters. Treat CLI parsing as an area to harden if arguments are operationally exposed.

### `startserver.h` / `startserver.cpp`

**Role:** Compose server and logger, manage the top-level listener/accept sequence.

**Types and interfaces:**

- `StartServer(const string&, const string&, const string&)`
- `int Run()`
- Holds a `Logger&` and `unique_ptr<ChatServer>`.

The constructor obtains the logger through `LoggerFactory` and asks `chatServerFactory` for a platform-specific server. `Run()` checks `getIsConnected()`, calls `createListner()`, enters `acceptConnections()`, closes client sockets, logs shutdown, and returns a process status. The header declares `readFromClientThread` and `serverBroadcastThread`, but the current implementation does not use them.

### `chatServerFactory.h`

**Role:** Compile-time construction of the platform backend.

`chatServerFactory::getInstance(Logger&, const string&, const string&)` returns:

- `HandleConnectionsWindows` when `_WIN32` is defined.
- `HandleConnectionsLinux` otherwise.

The factory returns `unique_ptr<ChatServer>`, allowing the entry/lifecycle code to call the common interface without knowing the concrete handler type.

### `chatserver.h` / `chatserver.cpp`

**Role:** Shared networking state, listener setup, client registry, outbound queue, and broadcast thread.

**Important declarations:**

- Constants: `MAX_THREAD` (4), `MAX_PORT_TRIES` (10), `MAX_QUEUE_CONNECTINON` (10), and `BUFFER_SIZE` (1024).
- Abstract `ChatServer::acceptConnections()`.
- `createListner()`, `closeSocket(int)`, `closeAllClientSockets()`, `getClientIP(int)`, `getIsConnected()`, `setIsConnected(bool)`, `getClientSockets()`, and `addProadcastMessage(int, const string&)`.

**Important state:**

- `m_SockfdListener`, server name, and port.
- Atomic connection flag.
- `m_ClientSockets` and `m_Mutex`.
- `m_BroadcastMessageQueue`, `m_BroadcastMutex`, and `m_Cv`.
- `m_BroadcastThread` and a non-owning `Logger&`.

**Behavior:**

- `createListner()` resolves the bind endpoint, creates a TCP socket, sets `SO_REUSEADDR`, binds an address candidate, and calls `listen`.
- `addProadcastMessage()` stores a `(socket, message)` pair and notifies the broadcast worker.
- `threadBroadcastMessage()` waits for queue entries and calls `sendProdcastMessage()`.
- `sendProdcastMessage()` retries up to five times with a 500 ms delay.
- `sendMessage()` prepends a four-character decimal message length and performs one `send` call.
- `closeAllClientSockets()` closes descriptors in the registry and clears it.

`MAX_PORT_TRIES` is not used. Listener setup loops over `getaddrinfo` results, but it does not implement port retries. `createListner()` calls `exit(EXIT_FAILURE)` on address resolution failure rather than returning an error to its caller.

### `handleConnectionsLinux.h` / `handleConnectionsLinux.cpp`

**Role:** Linux listener/client readiness and client reads.

**Important interface:** `HandleConnectionsLinux` overrides `acceptConnections()` and exposes `handleClient(int)`, `createEpollInstance()`, and `makeSocketNonBlocking(int)`.

**Runtime behavior:**

- Uses `epoll_create1`, `epoll_ctl`, and blocking `epoll_wait`.
- Makes listener and accepted client sockets nonblocking through `fcntl`.
- Accepts new clients when the listener is reported readable.
- Enqueues client-read lambdas on `ThreadPool`.
- `handleClient()` performs a single `read` into a `BUFFER_SIZE` buffer. On positive data it queues the bytes for every other socket; on EOF it removes the socket from epoll and closes it.

The constructor creates an epoll descriptor and tries registering the listener before `createListner()` is called by `StartServer`. `acceptConnections()` then calls `createEpollInstance()`, creating/registering a second epoll descriptor. The initial descriptor is overwritten without an explicit close. The active setup uses level-triggered `EPOLLIN`; the constructor also includes an initial `EPOLLIN | EPOLLOUT | EPOLLET` registration attempt.

The constructor also creates a `ThreadPool`, and `createEpollInstance()` assigns another pool before accepting begins. Replacing the first pool destroys it; this duplicate initialization is unnecessary.

The handler does not assemble protocol frames, distinguish retryable `EAGAIN`/`EWOULDBLOCK` from other read failures, or robustly handle accept/epoll registration errors. These are behavior gaps, not documented guarantees.

### `handleConnectionsWindows.h` / `handleConnectionsWindows.cpp`

**Role:** Windows Winsock accept loop, IOCP association, and asynchronous client reads.

**`ClientContext`:**

- Holds the `OVERLAPPED` structure and `WSABUF`.
- Tracks `READ_HEADER` or `READ_BODY`, expected byte count, received byte count.
- Uses a fixed `BUFFER_SZ` of 2048 bytes.

**Important methods:**

- `createIoCompletionPort()`: creates an IOCP, associates the listener, and launches `MAX_THREAD` worker threads.
- `associateSocket(SOCKET)`: associates a client, allocates a context, and posts a header receive.
- `workerThread(HANDLE)`: waits with `GetQueuedCompletionStatus` and dispatches completions.
- `handleCompletion(...)`: advances header/body state, broadcasts a complete body, and starts the next header read.
- `repostRecv(...)` / `postReadHeader(...)`: continue body/header receives.
- `acceptConnections()`: accepts clients, records them, logs peer information, and associates them with IOCP.

The handler calls `WSAStartup` in its constructor and `WSACleanup` in its destructor. Header parsing uses `atoi`; body length is not checked against the fixed buffer capacity. Some receive-error paths log an error but do not consistently remove/close the client or free its context.

## Interfaces and shared dependencies

| Include/target | Purpose |
|---|---|
| `../utils/logger.h` / `logger` | Logger API, formatting, async file output, singleton factory. |
| `../utils/util.h` / `util` | Platform error codes/descriptions and logging helpers. |
| `../utils/threadPool.h` and Linux sources | Linux client-read task execution. |
| `../utils/functionWrapper.h/.cpp` | Included in the Linux server source list and used by the alternate `threadPool_` implementation. |
| `clientsocket/clientSocket.h/.cpp` | Companion client; clarifies expected framing but is built separately. |

Platform-specific dependencies:

- **Windows:** Winsock2, `ws2tcpip.h`, Windows IOCP APIs, and link library `ws2_32`.
- **Linux/Unix:** POSIX sockets, epoll, `fcntl`, `unistd`, pthreads, and `fmt` for formatting.
- **Both:** C++20 standard library and a C++20-capable compiler.

## Configuration, environment, and secrets

There are no environment-variable or configuration-file settings in the inspected server code. Runtime settings are positional arguments:

| Argument | Default | Meaning |
|---|---|---|
| `server_name` | `localhost` | Hostname/address passed to `getaddrinfo` for the listener. |
| `port_number` | `8080` | TCP port; intended valid range is 1–65535. |
| `log_file_name` | `chatServer.log` | Filename below `<current working directory>/log/`. |

Other constants and operational values:

| Value | Definition |
|---|---|
| Log rotation threshold | 10,000,000 bytes (`MAX_LOG_FILE_SIZE` in `utils/logger.h`). |
| Linux epoll event array size | 10 (`MAX_QUEUE_CONNECTINON`). |
| Linux receive buffer | 1024 bytes (`BUFFER_SIZE`). |
| Windows IOCP worker count | 4 (`MAX_THREAD`). |
| Windows per-client receive buffer | 2048 bytes (`ClientContext::BUFFER_SZ`). |

No credentials, keys, tokens, or other secrets are required or managed by this server. It has no TLS or authentication configuration. Restrict network exposure until security controls are added.

## Build and deployment

### Toolchain

- CMake 3.15+ for the root project; the chatserver subdirectory declares 3.10 and its preset file requires CMake 3.19.
- C++20 compiler.
- Ninja if using the provided chatserver presets or the commands below.
- Windows: Winsock system library.
- Linux: pthreads and a discoverable `fmt` CMake package.

The root CMake project adds `utils` and `clientsocket`; `chatserver/CMakeLists.txt` links the `logger` and `util` targets from the utility subdirectory. The root project does not define its own `CMakePresets.json`. Configure and build the repository from its root:

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Debug configuration:

```text
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
```

The presets in `chatserver/CMakePresets.json` configure the subdirectory alone, while its CMake file expects utility targets normally provided by the root build. Prefer root-level configuration unless dependencies are otherwise made available.

Run the resulting `chatServer` executable with up to three positional arguments:

```text
chatServer 0.0.0.0 8080 chatServer.log
```

The binary's exact output path depends on the CMake generator and build directory. The process needs permission to bind the selected address/port and create or write `<current working directory>/log/`. No install target, service definition, container image, or packaging rules are defined.

Build-specific observations:

- On non-Windows, `chatserver/CMakeLists.txt` sets `CMAKE_PREFIX_PATH` to a path containing `x64-windows/share/fmt`, which may not match the host's Linux package location.
- It defines both `startServer` and `chatServer` from the same sources; verify whether both targets are intended.
- The chatserver presets include a third configure preset with an empty toolchain value; standard `default` and `debug` presets are clearer.
- CTest is enabled, but no chatserver tests are defined.

## Coding patterns and conventions

- **Abstract base plus platform subclasses:** `ChatServer` defines common state/operations and a pure virtual accept loop; handlers provide platform I/O.
- **Compile-time selection:** `_WIN32` guards select headers, socket types, error handling, and factory choice.
- **Singleton factories:** `LoggerFactory` provides a process-level logger instance. Server objects themselves are constructed through `chatServerFactory`.
- **Producer/consumer queue:** shared broadcast queue with mutex and condition variable separates message production from delivery.
- **Thread ownership:** uses `std::jthread` for broadcast/logger/Windows workers and a custom `ThreadPool` for Linux read tasks.
- **Logging:** structured format strings and log levels (`Debug`, `Info`, `Warning`, `Error`) are used throughout.
- **Naming:** method and type names are mixed-case; existing misspellings such as `createListner`, `addProadcastMessage`, and `sendProdcastMessage` are part of the current interface and should be changed consistently if corrected.

The code also has repeated platform socket definitions and exposes shared mutable state to derived classes. Resource ownership and synchronization conventions should be clarified as part of future refactoring.

## Error handling, logging, and monitoring

### Error handling

- Listener address-resolution failure is logged and terminates the process via `exit`.
- Bind/listen, socket-option, and selected epoll/IOCP failures are logged; not all failure paths return consistently or clean up all resources.
- The Windows receive path checks `WSA_IO_PENDING` for overlapped calls and logs some other failures.
- Linux `read` handles positive bytes and EOF explicitly, but does not robustly handle negative return values or distinguish retryable nonblocking errors.
- The shared sender treats any nonnegative `send` result as success, even if only part of the framed message was sent.
- Broad exception-based recovery is not a central pattern in the server code.

### Logging

- `StartServer` gets a `Logger&` from `LoggerFactory`; the logger is shared by the server.
- Logger output is queued and written by a background `jthread`.
- Messages include a timestamp and log level; the file rotates after the configured size threshold.
- The logger builds its path from the process current directory and creates a `log` directory if needed.
- Logger shutdown is requested from server destruction paths and handled by `Logger::stopProcessing()` at logger destruction.

Shutdown signaling and condition-variable notification should be tested carefully: the broadcast worker can be waiting on an empty queue while stop is requested, and the current code does not visibly notify that condition variable on stop. The derived and base server destructors also repeat listener, broadcast, and logger shutdown actions.

### Monitoring

The implementation provides file logs but no metrics endpoint, health check, tracing, external log aggregation, or alert integration. Operationally useful additions include active connection count, queue depth/bytes, per-client send failures, accept/read errors, worker utilization, and shutdown duration.

## Testing strategy and coverage

No automated test cases or server-specific CTest targets are present in the inspected server directory. CMake calls `include(CTest)` and `enable_testing()`, but that alone does not add coverage.

The companion client can support manual smoke testing: start the server, connect two or more clients, send messages, and confirm each sender is excluded while other clients receive messages. This does not cover all TCP behaviors or platform equivalence.

Recommended automated coverage:

1. Unit tests for four-byte header encoding/decoding, malformed headers, empty messages, maximum payload, and over-limit payload.
2. Socket-level tests that split a header/body across writes and combine multiple frames in one write.
3. Integration tests for both backends covering connect/disconnect, broadcasts to multiple clients, and peer removal.
4. Error-path tests for listener bind failure, client reset, partial send, nonblocking `EAGAIN`, and invalid frame lengths.
5. Shutdown tests with idle connections, pending broadcasts, and active reads, run repeatedly to detect deadlocks and resource leaks.
6. Cross-platform CI builds and tests; add sanitizer or static-analysis runs where supported.

## Implementation priorities

1. Unify frame parsing/encoding and reject invalid or oversized payloads.
2. Implement complete read/write handling for TCP streams and handle retryable socket errors.
3. Centralize socket/context ownership with RAII and remove duplicate destructor cleanup.
4. Define stop signaling and joining behavior for broadcast, epoll, IOCP, and logger workers.
5. Synchronize access to the client registry and prevent sends to closed/reused descriptors.
6. Bound client count and outbound queue memory; isolate slow recipients.
7. Add automated protocol, integration, and lifecycle tests before performance tuning.
