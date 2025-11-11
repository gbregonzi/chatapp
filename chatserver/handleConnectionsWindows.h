#pragma once

// #include <winsock2.h>
// #include <ws2tcpip.h>
// #include <windows.h>
// #include <string>


#include "chatserver.h"
#include "../utils/logger.h"

using namespace std;

class HandleConnectionsWindows : ServerSocket {
private:
    SOCKET m_sockfdListener;
    HANDLE m_IOCP;
public:
    // Constructor
    // logger: reference to Logger instance for logging
    // serverName: the server hostname or IP address
    // portNumber: the port number to bind the server socket
    HandleConnectionsWindows(Logger &logger, const string& serverName, const string& portNumber)
        : ServerSocket(logger, serverName, portNumber) {}

    // WorkerThread - worker thread function for handling IOCP events
    // iocp: the IO completion port handle
    void WorkerThread(HANDLE iocp) override;

    // AssociateSocket - 
    void AssociateSocket(SOCKET clientSocket) override;

    // AcceptConnections - accepts new client connections (Windows IOCP version)
    void AcceptConnections() override;
    
    // handleSelectConnections - handles multiple client connections using select()
    void handleSelectConnections() override;
};