#pragma once

#include <netinet/in.h>

#include "chatserver.h"
#include "../utils/logger.h"

using namespace std;

typedef void *HANDLE;
typedef unsigned long long SOCKET;

class HandleConnectionsLinux: public chatServer{
private:
    SOCKET m_SockfdListener;
public:
    // Constructor
    // logger: reference to Logger instance for logging
    // serverName: the server hostname or IP address
    // portNumber: the port number to bind the server socket
    HandleConnectionsLinux(Logger &logger, const std::string& serverName, const std::string& portNumber);

    // WorkerThread - worker thread function for handling IOCP events
    // iocp: the IO completion port handle
    void WorkerThread(HANDLE iocp);

    // // AssociateSocket - 
    void AssociateSocket(uint64_t clientSocket) override;
   
    // // AcceptConnections - Waiting for client connections
    void AcceptConnections() override;

    // HandleConnectionsWindows - Initialize the server connection listner socket
    bool createListner() override;
    
    ~HandleConnectionsLinux();
};
