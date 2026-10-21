# Chat Server System Architecture

## Scope

This document describes the system-level design implemented in the repository's `chatserver` directory. It covers module boundaries, process/runtime structure, network and message flows, dependencies, deployment assumptions, and constraints. For class-level details, build commands, logging, and tests, see [the engineering guide](../engineering/chatserver-engineering.md).

The codebase is a C++20 TCP chat server with Windows and Linux-specific connection handling. It accepts multiple TCP clients in one server process and is intended to relay a client's message to the other connected clients. The Linux and Windows handlers currently do not implement equivalent receive/framing behavior; descriptions below distinguish intended behavior from what the source currently does.

## System context and architecture

```text
                         +-------------------------+
                         | Chat client             |
                         | clientsocket (example)  |
                         +------------+------------+
                                      |
                              TCP / custom framing
                                      |
                                      v
+-------------------------------------------------------------------+
| chatServer process                                                |
|                                                                   |
|  main -> StartServer -> platform factory -> ChatServer            |
|                                     |                             |
|                    +----------------+----------------+            |
|                    |                                 |            |
|                    v                                 v            |
|          Linux: epoll + pool             Windows: accept + IOCP   |
|                    |                                 |            |
|                    +----------------+----------------+            |
|                                     |                             |
|                   client messages -> in-process queue             |
|                                     |                             |
|                                     v                             |
|                       broadcast worker -> TCP clients             |
|                                                                   |
|  Logger worker -> local log file                                  |
+-------------------------------------------------------------------+
```

The server is a single executable and single process. The listener, connection handler, worker threads, broadcast queue, and logger are all in-process. The chatserver has no database, message broker, external application service, or remote logging integration in the inspected code.

## Major components and interactions

| Component | System responsibility | Interaction |
|---|---|---|
| `main` / `StartServer` | Read startup arguments and own top-level startup/shutdown flow. | Creates the logger and server object; initializes the listener; runs the selected connection loop. |
| `chatServerFactory` | Select the platform-specific implementation at compile time. | Returns a Linux or Windows subclass through the `ChatServer` interface. |
| `ChatServer` | Own common listener configuration, client registry, message queue, and outgoing broadcast worker. | Receives completed messages from its subclass; schedules a send to each recipient other than the sender. |
| `HandleConnectionsLinux` | Detect socket readiness and dispatch client reads. | Uses Linux `epoll`, then submits client work to the internal thread pool. |
| `HandleConnectionsWindows` | Accept sockets and process overlapped reads. | Uses Winsock and an IO Completion Port (IOCP) with completion workers. |
| `Logger` | Persist diagnostic and operational log messages asynchronously. | Called by startup, shared server code, and connection handlers. |
| Chat clients | Connect and exchange framed messages. | TCP peers; the repository's `clientsocket` is an example client, not a server-side service dependency. |

### Key architectural pattern

Connection handling and outgoing delivery are decoupled by an in-process producer/consumer queue. A platform handler identifies a sender and message, then calls the shared queue method once for each other client. A single `ChatServer` broadcast thread consumes the queued `(recipient socket, message)` items and sends them. This avoids performing all recipient sends inline on the connection-read path, but the one shared sender can become a throughput bottleneck or be delayed by a slow client.

## Data flow and protocol

### Intended message flow

```text
Client A
  | TCP connect
  v
Listener -> accept -> register A
  |
  | framed message
  v
Platform receive handler
  |
  | decode message (intended)
  v
For each connected client B != A:
  enqueue (B, message)
  |
  v
Shared broadcast worker -> encode frame -> TCP send to B
  |
  v
Client B reads header, then payload
```

The companion `ClientSocket` uses a four-character, zero-padded decimal byte length followed by that many payload bytes (e.g. `0005hello`). The shared server send helper follows the same general format. A four-character decimal header can represent lengths from 0 through 9999, but the format is not declared in a shared protocol module.

### Current implementation differences

- **Windows:** The handler reads a four-byte header and then a body, supporting partial header/body receives through `ClientContext`. The declared body length is not validated against the fixed 2048-byte buffer before use.
- **Linux:** The handler reads once into a 1024-byte buffer and broadcasts the received bytes as-is. It does not decode the four-byte header or reassemble fragmented/coalesced TCP data.
- **Both:** The shared outgoing helper adds a length header. Because Linux passes inbound wire bytes directly into it, the Linux output is not equivalent to Windows output.

TCP is a byte stream; individual `send` and `read` calls do not define message boundaries. The protocol and its handling therefore need to be unified before Linux and Windows can be considered interoperable for all message sizes and network conditions.

## Runtime topology

### Linux process

```text
Main / StartServer thread
  ├─ construct shared server and logger
  ├─ open TCP listener
  └─ acceptConnections
       ├─ epoll_wait(listener and client sockets)
       ├─ accept ready connections
       └─ enqueue client-read tasks
            └─ ThreadPool worker -> handleClient
                 └─ enqueue one broadcast item per recipient

ChatServer broadcast jthread
  └─ dequeue item -> send to recipient

Logger jthread
  └─ dequeue log records -> write/rotate local file
```

The Linux handler creates a worker pool sized from `std::thread::hardware_concurrency()`. Its active event setup uses level-triggered `EPOLLIN`, with an event batch size of 10.

### Windows process

```text
Main / StartServer thread
  ├─ construct server; initialize Winsock
  ├─ open TCP listener
  └─ blocking acceptConnections
       └─ associate client with IOCP and post overlapped receive
            └─ one of four IOCP worker threads
                 └─ assemble message -> enqueue per-recipient broadcasts

ChatServer broadcast jthread
  └─ dequeue item -> send to recipient

Logger jthread
  └─ dequeue log records -> write/rotate local file
```

The listener accept loop and the IOCP completion workers coexist in the server process. Each client has a `ClientContext` holding its overlapped operation and read state.

## External APIs and dependencies

| Dependency | Use |
|---|---|
| TCP/IP sockets | Client-server transport, using the operating system socket API. |
| Windows Winsock2 / `ws2_32` | Windows socket lifecycle and asynchronous socket operations. |
| Windows IO Completion Ports | Multiplexing Windows overlapped I/O completions. |
| Linux/POSIX sockets, `epoll`, `fcntl`, `unistd` | Linux TCP listener, readiness notification, and nonblocking descriptors. |
| C++20 standard library | Threads, atomics, containers, filesystem, formatting on Windows, and RAII primitives. |
| `fmt` | Non-Windows formatting used by the logger. |
| CMake and Ninja | Project configuration and build orchestration. |

The code resolves bind addresses with `getaddrinfo` using `AF_UNSPEC` and `SOCK_STREAM`. The repository's `clientsocket` implementation provides an internal example of the expected protocol. The server does not call a third-party chat API or depend on an external runtime service.

## Deployment architecture and assumptions

### What is implemented

- One manually launched server executable per instance.
- A TCP listener on a hostname/address and port supplied as command-line arguments.
- A local filesystem log file under `<process working directory>/log/`.
- OS-native networking and threading; no container, service-manager, orchestration, install, or packaging configuration is provided in the inspected server directory.

### Environment assumptions

- The process can bind the requested address and port, and the host firewall/network permits client connections.
- The process current directory is stable and writable so the logger can create/use the `log` directory.
- A C++20-capable toolchain and the platform-specific build dependencies are available.
- A single instance owns its in-memory client list and broadcasts. Cross-instance client discovery or message distribution is not implemented.

The default bind name is `localhost`, which is typically loopback-only and therefore not externally reachable. Binding to `0.0.0.0` is an example of listening on all IPv4 interfaces, not a secure default. The code does not include TLS, authentication, authorization, or access-control policy; deployment on untrusted networks requires those capabilities or an appropriate trusted network boundary.

No environment-variable-based settings, secret store, database credentials, or external service credentials were found in the inspected server implementation.

## Architectural constraints and rationale

| Constraint / choice | Current rationale or consequence |
|---|---|
| Platform-specific I/O handlers behind a shared base class | Allows the build to select epoll on Linux and IOCP on Windows while sharing listener and broadcast responsibilities. |
| TCP stream transport | Provides ordered reliable bytes but requires explicit framing and handling of partial reads/writes. |
| In-memory per-process client set and broadcast queue | Simple single-instance relay; no persistence or horizontal coordination. |
| One shared outgoing broadcast worker | Separates reads from writes, but serializes outgoing work and can be blocked by slow clients. |
| Fixed or hardware-derived worker counts | Avoids external worker management but is not tuned to load and can vary by host. |
| Build-time platform selection | Only one platform handler is compiled into an executable; there is no runtime backend switch. |
| Local asynchronous file logging | Keeps log writes off the main call path, but couples log persistence to local filesystem availability and process working directory. |

## Architectural risks and recommended direction

1. Specify and share a protocol codec across the client and both server backends. Validate frame lengths, cap payloads, and test fragmentation and coalescing.
2. Give each client bounded outbound buffering and independent write readiness/completion handling, so one slow recipient does not block other broadcasts.
3. Define explicit stop/start lifecycle ownership for listener, workers, queues, sockets, and logger; test shutdown while idle and under load.
4. Bound clients, message rates, payload sizes, and queued bytes to control resource use.
5. Add TLS and an identity/authorization model if deployment is not restricted to a trusted network.
6. If multiple instances are required, define client routing and a broker or other inter-instance fan-out design; the current in-memory topology cannot provide cross-instance broadcasts.
7. Add metrics and health reporting for connection count, queue depth, send/read failures, and worker saturation.

These recommendations are based on source inspection, not a production deployment specification or a security audit.
