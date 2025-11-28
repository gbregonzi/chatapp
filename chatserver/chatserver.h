#pragma once

#include <string_view>
#include <atomic>
#include <queue>
#include <unordered_set>
#include <mutex>
#include <thread>
#include <iostream>
#include <condition_variable>
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
#else
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <sys/epoll.h>
    #include <netinet/in.h>
    typedef void *HANDLE;
    typedef unsigned long long SOCKET;
#endif
#include <string>

#include "../utils/logger.h"
#include "../utils/util.h"

using namespace std;

constexpr int MAX_THREAD{4};
constexpr int MAX_PORT_TRIES{10};
constexpr int MAX_QUEUE_CONNECTINON{10};
constexpr int BUFFER_SIZE{1024};

class ChatServer {
    protected:
#ifdef _WIN32
        SOCKET m_SockfdListener;
        char optval{1};
#else
        int m_SockfdListener;
        int optval{1};
#endif
        string m_ServerName;
        string m_PortNumber;
    private:
        atomic<bool> m_IsConnected{false};
        bool sendMessage(int sd, const string_view message);

        // threadBroadcastMessage - processes messages from the read message queue
        void threadBroadcastMessage();
    protected:
        jthread m_BroadcastThread;
        queue<pair<int, string>> m_BroadcastMessageQueue;
        unordered_set<int> m_ClientSockets;
        vector<jthread> m_Threads;
        mutex m_Mutex;
    public:
        Logger& m_Logger;
        // Constructor
        // logger: reference to Logger instance for logging
        // serverName: the server hostname or IP address
        // portNumber: the port number to bind the server socket
        ChatServer(Logger &logger, const string& serverName, const string& portNumber);
        
        // getClientIP - retrieves and prints the connected client's IP address and port
        // sd: the socket descriptor of the connected client
        // Returns true if successful, false otherwise
        bool getClientIP(int sd);

        // getIsConnected - returns the connection status
        bool getIsConnected() const;
        
        // setIsConnected - sets the connection status
        void setIsConnected(bool isConnected);
                      
        // // AcceptConnections - accepts new client connections (Windows IOCP version)
        virtual void acceptConnections() = 0;

        // createListner - Initialize the server connection listner socket
        bool createListner();

        // closeSocket - closes the client socket 
        // sd: the socket descriptor 
        void closeSocket(int sd);

        // closeAllClientSockets - closes all connected client sockets
        void closeAllClientSockets();

        // getClientSockets - returns the set of currently connected client sockets 
        // Returns an unordered_set of client socket descriptors
        unordered_set<int> getClientSockets() const;
};
