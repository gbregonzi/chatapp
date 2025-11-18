#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>


#include "chatserver.h"
#include "../utils/logger.h"
#include "../utils/util.h"

using namespace std;

class HandleConnectionsWindows : public chatServer{
private:
    HANDLE m_IOCP;
public:
    // Constructor
    // logger: reference to Logger instance for logging
    // serverName: the server hostname or IP address
    // portNumber: the port number to bind the server socket
    HandleConnectionsWindows(Logger &logger, const std::string& serverName, const std::string& portNumber);

    // WorkerThread - worker thread function for handling IOCP events
    // iocp: the IO completion port handle
    void WorkerThread(HANDLE iocp);

    // // AssociateSocket - 
    void AssociateSocket(unsigned int64_t clientSocket) override;
   
    // // AcceptConnections - Waiting for client connections
    void AcceptConnections() override;

    // HandleConnectionsWindows - Initialize the server connection listner socket
    bool createListner() override;
    
    ~HandleConnectionsWindows();
};
