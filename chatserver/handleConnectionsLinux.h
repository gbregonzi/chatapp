#pragma once

#include <netinet/in.h>
#include <sys/epoll.h>

#include "chatserver.h"
#include "../utils/logger.h"

using namespace std;

typedef void *HANDLE;
typedef unsigned long long SOCKET;

constexpr int MAX_EVENTS = 10;

class HandleConnectionsLinux: public ChatServer{
private:
    SOCKET m_epollFd;
    struct epoll_event ev, events[MAX_EVENTS];
    
public:
    // Constructor
    // logger: reference to Logger instance for logging
    // serverName: the server hostname or IP address
    // portNumber: the port number to bind the server socket
    HandleConnectionsLinux(Logger &logger, const string& serverName, const string& portNumber);

    //Destructor
    ~HandleConnectionsLinux();
    
    // WorkerThread - worker thread function for handling IOCP events
    // iocp: the IO completion port handle
    void workerThread(HANDLE iocp);

    // AssociateSocket - 
    void associateSocket(uint64_t clientSocket) override;
   
    // AcceptConnections - Waiting for client connections
    void acceptConnections() override;
};
