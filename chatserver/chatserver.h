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
    #include <netinet/in.h>
#endif

#include "../utils/outputStream.h"
#include "../utils/threadPool.h"
#include "../utils/logger.h"

using namespace std;
constexpr int BUFFER_SIZE{1024};

enum class messageType { 
    HTTP_HEADER, 
    BAD_REQUEST,
    NOT_FOUND 
};

string const HEADER_MESSAGES[] = {
    "HTTP/1.1 200 OK\r\n",
    "HTTP/1.1 400 Bad Request\r\n"
    "Content-Type: text/html\r\n"
    "\r\n"
    "<html>\r\n"
    "<head><title>400 Bad Request</title></head>\r\n"
    "<body><h1>400 Bad Request</h1><p>Your browser sent a request that this server could not understand.</p></body>\r\n"
    "</html>\r\n",
    "HTTP/1.1 404 File not found\r\n"
    "\r\n"
    "<html>\r\n"
    "<head><title>404 File not found</title></head>\r\n"
    "<body><h1>404 File not found</h1><p>The requested file doesn't exist on this server.</p></body>\r\n"
    "</html>\r\n",
    "Content-Type: text/html\r\n"
};

string const FILE_EXTENSIOS[] = {
    "jpg","jpeg","png","gif","css","js","json","xml","pdf",
    "zip","mp3","mp4","avi","mov","wmv","flv","svg","ico",
    "html","htm","txt","csv","tsv","md","mpg","mpeg","mkv"
    };

string const FILE_MIME_TYPES[] = {
    "Content-Type: image/jpeg\r\n",
   "Content-Type: image/jpeg\r\n",
    "Content-Type: image/png\r\n",
    "Content-Type: image/gif\r\n",
    "Content-Type: text/css\r\n",
    "Content-Type: application/javascript\r\n",
    "Content-Type: application/json\r\n",
    "Content-Type: application/xml\r\n",
    "Content-Type: application/pdf\r\n",
    "Content-Type: application/zip\r\n",
    "Content-Type: audio/mpeg\r\n",
    "Content-Type: video/mp4\r\n",
    "Content-Type: video/x-msvideo\r\n",
    "Content-Type: video/quicktime\r\n",
    "Content-Type: video/x-ms-wmv\r\n",
    "Content-Type: video/x-flv\r\n",
    "Content-Type: image/svg+xml\r\n",
    "Content-Type: image/x-icon\r\n",
    "Content-Type: text/html\r\n",
    "Content-Type: text/html\r\n",
    "Content-Type: text/plain\r\n", 
    "Content-Type: text/csv\r\n",
    "Content-Type: text/tab-separated-values\r\n",
    "Content-Type: text/markdown\r\n",
    "Content-Type: video/mpeg\r\n",
    "Content-Type: video/mpeg\r\n",
    "Content-Type: video/x-matroska\r\n",
    "Content-Type: application/octet-stream\r\n",
    };

class ServerSocket{
    private:
    #if defined(_WIN32)
        SOCKET m_sockfdListener;
    #else
        int m_sockfdListener;
    #endif
        string m_ServerName;
        string m_PortNumber;
        atomic<bool> m_IsConnected{false};
        mutex m_Mutex;
        //condition_variable_any m_condVar; // For signaling new messages
        queue<pair<int, string>> m_BroadcastMessageQueue;
        //queue<string> m_SendMessageQueue;
        unordered_set<int> m_ClientSockets;
        jthread m_BroadcastThread;
        Logger& m_Logger;
        fd_set m_Master; // master file descriptor list
        vector<jthread> m_Threads;
        HANDLE m_IOCP;

        // sendMessage - sends a specific message to the connected client
        // sd: the socket descriptor of the connected client
        // message: the message to be sent
        // Retunrs true if successful, false otherwise
        bool sendMessage(int sd, const string_view message);

        // threadBroadcastMessage - processes messages from the read message queue
        void threadBroadcastMessage();

        // handleClient - handles communication with a specific client
        // fd: the socket descriptor of the connected client
        void handleClientMessage(int fd);

        // WorkerThread - worker thread function for handling IOCP events
        // iocp: the IO completion port handle
        void WorkerThread(HANDLE iocp);

        // AssociateSocket - 
        void AssociateSocket(SOCKET clientSocket);

    public:
        // Constructor
        // logger: reference to Logger instance for logging
        // serverName: the server hostname or IP address
        // portNumber: the port number to bind the server socket
        ServerSocket(Logger &logger, const string& serverName, const string& portNumber);
        
        // getClientIP - retrieves and prints the connected client's IP address and port
        // sd: the socket descriptor of the connected client
        // Returns true if successful, false otherwise
        bool getClientIP(int sd);

        // getClientIP - Overloaded version to get IP from sockaddr_storage
        // p: pointer to addrinfo structure
        bool getClientIP(addrinfo* p);

        // getIsConnected - returns the connection status
        bool getIsConnected() const;
        
        // setIsConnected - sets the connection status
        void setIsConnected(bool isConnected);
        
        // AcceptConnections - accepts new client connections (Windows IOCP version)
        void AcceptConnections();
        
        // handleSelectConnections - handles multiple client connections using select()
        void handleSelectConnections();
        
        // closeSocket - closes the client socket 
        // sd: the socket descriptor 
        void closeSocket(int sd);

        // closeSocketServer - closes the server socket
        void closeSocketServer();

        // closeAllClientSockets - closes all connected client sockets
        void closeAllClientSockets();

        // getClientSockets - returns the set of currently connected client sockets 
        // Returns an unordered_set of client socket descriptors
        unordered_set<int> getClientSockets() const;

        // getLastErrorDescription - retrieves a description for the last error code
        string getLastErrorDescription();

        // Destructor
        ~ServerSocket();
};

struct ServerSocketFactory{    
    inline static ServerSocket& getInstance(Logger &logger, 
                                            const string& serverName, const string& portNumber){
        static ServerSocket instance(logger, serverName, portNumber);
        return instance;
    };   
};
