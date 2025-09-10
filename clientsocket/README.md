# clientsocket

A simple C++ client socket application for connecting to a server over TCP/IP.

## Features

- Connects to a specified server and port
- Sends and receives messages
- Handles basic socket errors

## Requirements

- Linux OS
- g++ compiler
- Basic knowledge of C++ and sockets

## Build

```bash
g++ -o clientsocket main.cpp
```

## Usage

```bash
./clientsocket <server_ip> <port>
```

Example:

```bash
./clientsocket 127.0.0.1 8080
```

## Files

- `main.cpp` – Main source code for the client socket
- `README.md` – Project documentation

## License

MIT
