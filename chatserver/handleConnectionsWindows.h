#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>


#include "chatserver.h"
#include "../utils/logger.h"
#include "../utils/util.h"

using namespace std;

class HandleConnectionsWindows : public ChatServer{
private:
    HANDLE m_IOCP;
public:
    // Constructor
    // logger: reference to Logger instance for logging
    // serverName: the server hostname or IP address
    // portNumber: the port number to bind the server socket
    HandleConnectionsWindows(Logger &logger, const std::string& serverName, const std::string& portNumber);
    
    //Destructor
    ~HandleConnectionsWindows();
    
    // workerThread - worker thread function for handling IOCP events
    // iocp: the IO completion port handle
    void workerThread(HANDLE iocp);

    // // associateSocket - 
    void associateSocket(uint64_t clientSocket);
   
    // // acceptConnections - Waiting for client connections
    void acceptConnections() override;

    // CreateIoCompletionPort - Initialize IOCP connection
    void createIoCompletionPort();
    
};
