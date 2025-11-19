#pragma once
#include <sys/epoll.h>

#include "chatserver.h"
#include "../utils/logger.h"
#include "../utils/threadPool.h"

using namespace std;

typedef void *HANDLE;
typedef unsigned long long SOCKET;

class HandleConnectionsLinux: public ChatServer{
private:
    SOCKET m_epollFd;
    struct epoll_event m_Events[MAX_QUEUE_CONNECTINON];
    unique_ptr<ThreadPool> threadPool;
public:
    // Constructor
    // logger: reference to Logger instance for logging
    // serverName: the server hostname or IP address
    // portNumber: the port number to bind the server socket
    HandleConnectionsLinux(Logger &logger, const string& serverName, const string& portNumber);

    //Destructor
    ~HandleConnectionsLinux();
    
    // WorkerThread - worker thread function for handling IOCP events
    // clientDt: File descriptor for the client connected
    void handleClient(int clientFd);

    // AcceptConnections - Waiting for client connections
    void acceptConnections() override;

    // 
    int makeSocketNonBlocking(int sfd);
};
