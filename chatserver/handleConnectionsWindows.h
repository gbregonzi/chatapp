#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>


#include "chatserver.h"
#include "../utils/logger.h"
#include "../utils/util.h"

using namespace std;
struct ClientContext {
    OVERLAPPED overlapped;
    WSABUF wsabuf{};
    enum State { READ_HEADER, READ_BODY } state = READ_HEADER;
    size_t expected = 0;
    size_t received = 0;
    static constexpr int BUFFER_SZ = 2048;
    char buffer[BUFFER_SZ + 1];

    ClientContext() {
        ZeroMemory(&overlapped, sizeof(OVERLAPPED));
        wsabuf.buf = buffer;
        wsabuf.len = sizeof(buffer);
    }
};

class HandleConnectionsWindows : public ChatServer{
private:
    HANDLE m_IOCP;

    // handleCompletion - Handle completion of IO events for a client
    // ctx: Pointer to ClientContext containing client state
    // bytesTransferred: Number of bytes transferred in the IO operation
    // sd: Socket descriptor of the client  
    void handleCompletion(ClientContext* ctx, DWORD bytesTransferred, SOCKET sd);
    
    // postReadHeader - Post a read operation for the message header
    // sd: Socket descriptor of the client  
    void postReadHeader(SOCKET sd, ClientContext* ctx);

    // repostRecv - Repost a receive operation for the client
    // sd: Socket descriptor of the client  
    void repostRecv(SOCKET sd, ClientContext* ctx);
    
    // workerThread - worker thread function for handling IOCP events
    // iocp: the IO completion port handle
    void workerThread(HANDLE iocp);

    // // associateSocket - 
    void associateSocket(uint64_t clientSocket);
   
    // // acceptConnections - Waiting for client connections
    void acceptConnections() override;

    // CreateIoCompletionPort - Initialize IOCP connection
    void createIoCompletionPort();

public:
    // Constructor
    // logger: reference to Logger instance for logging
    // serverName: the server hostname or IP address
    // portNumber: the port number to bind the server socket
    HandleConnectionsWindows(Logger &logger, const std::string& serverName, const std::string& portNumber);
    
    //Destructor
    ~HandleConnectionsWindows();    
};
