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

    // createEpollInstance - Create epoll instance and setup thread pool
    // Returns 0 on success, -1 on failure
    int createEpollInstance();

    // AcceptConnections - Waiting for client connections
    void acceptConnections() override;

    // makeSocketNonBlocking - Make a socket non-blocking
    // sfd - Socket file descriptor to be made non-blocking
    // Returns 0 on success, -1 on failure
    int makeSocketNonBlocking(int sfd);
};
