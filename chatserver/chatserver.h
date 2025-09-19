#pragma once

#include <string_view>
#include <atomic>
#include <queue>
#include <unordered_set>
#include <mutex>
#include <iostream>
#include <condition_variable>
#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <netinet/in.h>
#endif

#include "../utils/outputStream.h"

using namespace std;

constexpr unsigned short PORT{8080};
constexpr int MAX_QUEUE_CONNECTINON{10};
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
        int m_sockfd;
        atomic<bool> m_IsConnected{false};
        struct sockaddr_in m_ServerAddr;
        mutex m_mutex;
        condition_variable_any m_condVar; // For signaling new messages
        queue<pair<int, string>> m_BroadcastMessageQueue;
        queue<string> m_SendMessageQueue;
        unordered_set<int> m_ClientSockets;
        stop_source m_Source;
        stop_token m_sToken;
        jthread m_BroadcastThread;
        unique_ptr<OutputStream>& m_cout;

        // sendMessage - sends a specific message to the connected client
        // sd: the socket descriptor of the connected client
        // message: the message to be sent
        // Retunrs true if successful, false otherwise
        bool sendMessage(int sd, const string_view message);

        // threadBroadcastMessage - processes messages from the read message queue
        void threadBroadcastMessage();

        // getHostNameIP - retrieves and prints the server's hostname and IP address
        void getHostNameIP();
        
    public:
        // Constructor
        ServerSocket(unique_ptr<OutputStream> &outputStream);
        
        // LogErrorMessage - prints error code and description
        // errorCode: the error code to be printed
        void logErrorMessage(int errorCode);

        // getClientIP - retrieves and prints the connected client's IP address and port
        // sd: the socket descriptor of the connected client
        // Returns true if successful, false otherwise
        bool getClientIP(int sd);

        // getIsConnected - returns the connection status
        bool getIsConnected() const;

        // getMutex - returns the mutex for external locking if needed
        mutex& getMutex() {
            return m_mutex;
        }   
        
        // setIsConnected - sets the connection status
        void setIsConnected(bool isConnected);

        // addBroadcastMessage - adds a message to the broadcast queue
        // message: the message to be added
        // sd: the socket descriptor of the client to send the message to
        // Returns 0 on success, -1 on failure
        int addBroadcastTextMessage(const string message, int sd);

        // addBroadcastMessage - adds a message to the broadcast queue
        // message: the message to be added
        // Returns 0 on success, -1 on failure
        int addBroadcastTextMessage(const string message);

        // handleConnections - handle incoming client connections
        // Returns the client socket descriptor on success, -1 on failure
        int handleConnections();
        
        // readMessage - reads a message from a specific client
        // message: output parameter to hold the received message
        // sd: the socket descriptor of the client to read from
        // Returns number of bytes read, or -1 on error/disconnection
        size_t readMessage(string &message, int sd);
        
        // getClientCount - returns the number of currently connected clients
        size_t getClientCount();

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

        // Destructor
        ~ServerSocket();

};

