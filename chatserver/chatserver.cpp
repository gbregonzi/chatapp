#ifdef _WIN32
    typedef int socklen_t;
    #define CLOSESOCKET closesocket
    #define ERROR_CODE WSAGetLastError()
#else
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #define CLOSESOCKET close
    #define ERROR_CODE errno
#endif
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>

#include "chatserver.h"

constexpr int MAX_PORT_TRIES{10};
constexpr int MAX_QUEUE_CONNECTINON{10};
constexpr int BUFFER_SIZE{1024};

using namespace std;

ServerSocket::ServerSocket(unique_ptr<OutputStream>& outputStream, const string& serverName, const string& port): 
                          m_cout(outputStream), m_ServerName(serverName), m_PortNumber(port)   
{
#ifdef _WIN32
    WSADATA d;
    if (WSAStartup(MAKEWORD(2,2), &d)) {
        *m_cout << __func__ << ":" << "Failed to initialize Winsock!\n";
        exit(EXIT_FAILURE);
    }
#endif
    struct addrinfo hints{}, *ai = nullptr, *p = nullptr;
    int fdMax; // maximum file descriptor number
    char hostName[INET6_ADDRSTRLEN];
    char service[20];
    memset (hostName, 0, sizeof(hostName));
    memset (service, 0, sizeof(service));

    FD_ZERO(&m_Master); // clear the master and temp sets
    memset(&hints, 0, sizeof(hints));
    
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo(serverName.c_str(), m_PortNumber.c_str(), &hints, &ai) != 0) {
        *m_cout << __func__ << ":Getaddrinfo failed!\n";
        *m_cout << __func__ << ":Error Code:" << ERROR_CODE << "\n";
        *m_cout << __func__ << ":" << gai_strerror(ERROR_CODE) << "\n";
        logErrorMessage(ERROR_CODE);
        exit(EXIT_FAILURE);
    }
    
    for (p = ai; p != nullptr; p = p->ai_next) {
        m_sockfdListener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (m_sockfdListener < 0) {
            continue;
        }
        
        char optval{1};
        int optlen{sizeof(optval)};
        if (setsockopt(m_sockfdListener, SOL_SOCKET, SO_REUSEADDR, &optval, optlen) < 0)
        {
            *m_cout << __func__ << ":" << "Setsockopt failed!\n";
            logErrorMessage(ERROR_CODE);
            CLOSESOCKET(m_sockfdListener);
            continue;
        }
        

        if (bind(m_sockfdListener, p->ai_addr, p->ai_addrlen) != -1) {
            break; // Success
        }
        
        *m_cout << __func__ << ":" << "Bind failed! Retrying...\n";
        logErrorMessage(ERROR_CODE);
        CLOSESOCKET(m_sockfdListener);
    }
    

    if (p == nullptr) {
        *m_cout << __func__ << ":" << "Failed to bind socket!\n";
        exit(EXIT_FAILURE);
    }
    
    *m_cout << __func__ << ":" << "Server created and bound successfully.\n";
    
    freeaddrinfo(ai); // all done with this

    if (listen(m_sockfdListener, MAX_QUEUE_CONNECTINON) < 0)
    {
        *m_cout << __func__ << ":" << "Listen failed!\n";
        logErrorMessage(ERROR_CODE);
        exit(EXIT_FAILURE);
    }
    if (getnameinfo(p->ai_addr, p->ai_addrlen, hostName, sizeof(hostName), service, sizeof(service), NI_NOFQDN|NI_NAMEREQD) != 0) {
    *m_cout << __func__ << ":" << "getnameinfo fail!\n";
    }
    *m_cout << __func__ << "Server name:" << hostName << " Port:" << service << "\n";
    m_IsConnected.store(true);
    threadBroadcastMessage();
}

void ServerSocket::handleSelectConnections() {
    struct sockaddr_storage remoteAddr; // client address
    fd_set readFds; // temp file descriptor list for select()
    int fdMax; // maximum file descriptor number
    FD_ZERO(&readFds);
    
    FD_SET(m_sockfdListener, &m_Master);
    fdMax = m_sockfdListener; 
    setIsConnected(true);
    
    while(getIsConnected()) {
        readFds = m_Master; // copy it
        if (select(fdMax+1, &readFds, nullptr, nullptr, nullptr) == -1) {
            *m_cout << __func__ << ":" << "Select failed!\n";
            logErrorMessage(ERROR_CODE);
            exit(EXIT_FAILURE);
        }
        
        for(int fd = 0; fd <= fdMax; fd++) {
            if (FD_ISSET(fd, &readFds)) { // we got one!!
                if (fd == m_sockfdListener) { // handle new connections
                    socklen_t clientAddrLen = sizeof(remoteAddr);
                    int  clientSocket = accept(m_sockfdListener, (struct sockaddr *)&remoteAddr, &clientAddrLen);
                    if (getIsConnected() == false)
                    {
                        *m_cout << __func__ << ":" << "Server is shutting down. Cannot accept new connections.\n"; 
                        CLOSESOCKET(clientSocket);
                        break;
                    }
                    if (clientSocket < 0)
                    {
                        *m_cout << __func__ << ":" << "Accept failed!\n";
                        logErrorMessage(ERROR_CODE);
                        continue;
                    }
                    
                    *m_cout << __func__ << ":" << " Client connected successfully.\n";
                    auto result = m_ClientSockets.emplace(clientSocket);
                    if (!result.second)
                    {
                        *m_cout << __func__ << ":" << " Failed to add client socket to the set.\n";
                        CLOSESOCKET(clientSocket);
                        continue;
                    }   
                    FD_SET(clientSocket, &m_Master); // add to master set
                    if (clientSocket > fdMax) {    // keep track of the max
                        fdMax = clientSocket;
                    }
                    getClientIP(clientSocket);
                } else {
                    handleClientMessage(fd, fdMax);
                }
            }
        }
    }
}

// handle data from a client
void ServerSocket::handleClientMessage(int fd, int fdMax) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    size_t bytes_received = recv(fd, buffer, sizeof(buffer) - 1, 0);
    if (getIsConnected() == false)
    {
        *m_cout << __func__ << ":" << "Server is shuting down. Cannot read messages.\n"; 
        CLOSESOCKET(fd);
        FD_CLR(fd, &m_Master); // remove from master set
        m_ClientSockets.erase(fd);
    }
    else {
        for(int j = 0; j <= fdMax; j++) {
            // send to everyone!
            if (FD_ISSET(j, &m_Master)) {
                // except the listener and ourselves
                if (j != m_sockfdListener && j != fd) {
                    lock_guard<mutex> lock(m_mutex);
                    auto result = m_BroadcastMessageQueue.emplace(make_pair(j, string(buffer)));
                    if (!result.first) {
                        *m_cout << __func__ << ":" << "Failed to add message to broadcast queue.\n";
                        continue;
                    }
                }
            }
        }

    }
}

// void ServerSocket::ServerSocket_(unique_ptr<OutputStream>& outputStream): m_cout(outputStream)
// {
// #ifdef _WIN32
//     WSADATA d;
//     if (WSAStartup(MAKEWORD(2,2), &d)) {
//         *m_cout << __func__ << ":" << "Failed to initialize Winsock!\n";
//         exit(EXIT_FAILURE);
//     }
// #endif
//     m_sockfd = socket(AF_INET, SOCK_STREAM, 0);
//     if (m_sockfd < 0)
//     {
//         m_cout->log("Socket creation failed!");
//         logErrorMessage(m_isWindows.load() ? WSAGetLastError() : errno);
//         exit(EXIT_FAILURE);
//     }
//     *m_cout << "m_cout:" << m_cout << "\n";

//     *m_cout << __func__ << ":" << "Server created successfully.\n";

//     memset(&m_ServerAddr, 0, sizeof(m_ServerAddr));

//     m_ServerAddr.sin_family = AF_INET;
//     m_ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
//     m_ServerAddr.sin_port = htons(PORT);
    

//     char optval{1};
//     int optlen{sizeof(optval)};
//     if (setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, optlen) < 0)
//     {
//         *m_cout << __func__ << ":" << "Setsockopt failed!\n";
//         logErrorMessage(m_isWindows.load() ? WSAGetLastError() : errno);
//         exit(EXIT_FAILURE);
//     }
//     auto randomPort = PORT;
//     auto attempts{0};
//     vector<int> triedPorts;
//     while (attempts < MAX_PORT_TRIES)
//     {
//         if (bind(m_sockfd, (struct sockaddr *)&m_ServerAddr, sizeof(m_ServerAddr)) < 0)
//         {
//             while (true)
//             {
//                 this_thread::sleep_for(chrono::milliseconds(500));
//                 randomPort = PORT + (rand() % 10); // Random port between 8080 and 8090
//                 m_ServerAddr.sin_port = htons(randomPort);
//                 if (find(triedPorts.begin(), triedPorts.end(), randomPort) == triedPorts.end())
//                 {
//                     triedPorts.push_back(randomPort);
//                     attempts++;
//                     break;
//                 }
//             }
//             *m_cout << __func__ << ":" << "Bind failed! Retrying..." << attempts << "/" << MAX_PORT_TRIES << "\n";
//             *m_cout << __func__ << ":" << "Trying port: " << randomPort << "\n";
//             logErrorMessage(m_isWindows.load() ? WSAGetLastError() : errno);
//             continue;
//         }
//         attempts++;
//         break;
//     }
//     *m_cout << __func__ << ":" << "Number of attempts to bind: " << attempts << "\n";
//     if (attempts == MAX_PORT_TRIES)
//     {
//         *m_cout << __func__ << ":" << "Failed to bind after multiple attempts!\n";
//         exit(EXIT_FAILURE);
//     }

//     *m_cout << __func__ << ":" << "Server bound to port " << randomPort << ".\n";

//     if (listen(m_sockfd, MAX_QUEUE_CONNECTINON) < 0)
//     {
//         *m_cout << __func__ << ":" << "Listen failed!\n";
//         logErrorMessage(m_isWindows.load() ? WSAGetLastError() : errno);
//         exit(EXIT_FAILURE);
//     }
//     getHostNameIP();

//     m_sToken = m_Source.get_token();
//     m_IsConnected.store(true);
//     threadBroadcastMessage();
// }

void ServerSocket::logErrorMessage(int errorCode)
{
    *m_cout << __func__ << ":Error Code: " << errorCode << "\n";
    *m_cout << __func__ << strerror(errorCode) << "\n\n";
}

mutex& ServerSocket::getMutex() {
    return m_mutex;
}

bool ServerSocket::getIsConnected() const
{
    return m_IsConnected.load();
}

void ServerSocket::setIsConnected(bool isConnected) 
{
    m_IsConnected.store(isConnected);
}

void ServerSocket::closeSocketServer() 
{
    CLOSESOCKET(m_sockfdListener);
    m_BroadcastThread.request_stop();
    *m_cout << __func__ << ":" << " Server socket closed.\n";    
}
void ServerSocket::closeAllClientSockets() 
{
    for (const auto &sd : m_ClientSockets){
        CLOSESOCKET(sd);
    }
    m_ClientSockets.clear();
    *m_cout << __func__ << ":" << " All client sockets closed.\n";    
}

bool ServerSocket::getIP(addrinfo* p){
    char ipStr[INET6_ADDRSTRLEN];
    void *addr;
    const char *ipVer;
    memset(ipStr, 0, sizeof(ipStr));

    // get the pointer to the address itself,
    // different fields in IPv4 and IPv6:
    if (p->ai_family == AF_INET) { // IPv4
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
        addr = &(ipv4->sin_addr);
        ipVer = "IPv4";
    } else { // IPv6
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
        addr = &(ipv6->sin6_addr);
        ipVer = "IPv6";
    }
    inet_ntop(p->ai_family, addr, ipStr, sizeof(ipStr));
    *m_cout << __func__ << ":" << ipVer << ": " << ipStr << "\n";
    return true;
}

bool ServerSocket::getClientIP(int sd )
{
    char ip[INET6_ADDRSTRLEN];
    int port;
    struct sockaddr_storage sockAddr;
    socklen_t sockAddrLen = sizeof(sockAddr);
    memset(&sockAddr, 0, sizeof(sockAddr));
    const char *ipVer;
    if (getpeername(sd, (struct sockaddr *)&sockAddr, &sockAddrLen) != 0)
    {
        *m_cout << __func__ << ":" << " Getpeername failed! Error: " << errno << "\n";
        return false;
    }
    
    if (sockAddr.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)&sockAddr;
        port = ntohs(s->sin_port);
        inet_ntop(AF_INET, &s->sin_addr, ip, sizeof(ip));
        ipVer = "IPV4:";
    } else { // AF_INET6
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&sockAddr;
        port = ntohs(s->sin6_port);
        inet_ntop(AF_INET6, &s->sin6_addr, ip, sizeof(ip));
        ipVer = "IPV6:";
    }
    *m_cout << __func__ << ":" << ipVer << ip << " Socket: " << port << "\n";
    return true;
}

int ServerSocket::handleConnections()
{
    *m_cout << __func__ << ":" << "Server is ready to accept connections.\n";
    socklen_t clientAddrLen = sizeof(sockaddr);
    int  clientSocket = accept(m_sockfdListener, (struct sockaddr *)&m_sockfdListener, &clientAddrLen);
    if (getIsConnected() == false)
    {
        *m_cout << __func__ << ":" << "Server is shutting down. Cannot accept new connections.\n"; 
        return EXIT_SUCCESS;
    }
    
    if (clientSocket < 0)
    {
        *m_cout << __func__ << ":" << "Accept failed!\n";
        logErrorMessage(ERROR_CODE);
        return EXIT_FAILURE;
    }
    
    if (!getClientIP(clientSocket))
    {
        return EXIT_FAILURE;
    }        
    
    *m_cout << __func__ << ":" << " Client connected successfully.\n";
    auto result = m_ClientSockets.emplace(clientSocket);
    if (!result.second)
    {
        *m_cout << __func__ << ":" << " Failed to add client socket to the set.\n";
        CLOSESOCKET(clientSocket);
        return EXIT_FAILURE;
    }   
    return clientSocket;
}

ServerSocket::~ServerSocket()
{
    CLOSESOCKET(m_sockfdListener);
#ifdef _WIN32
     WSACleanup();
#endif
     *m_cout << __func__ << ":" << " ServerSocket class destroyed.\n";
}

bool ServerSocket::sendMessage(int sd, const string_view message)
{
    size_t bytesSent = send(sd, message.data(), message.length(), 0);
    if (bytesSent < 0)
    {
        *m_cout << __func__ << ":" << " Send to client failed!\n";
        logErrorMessage(ERROR_CODE);
        return false;
    }
    return true;
}

size_t ServerSocket::readMessage(string &message, int sd){
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    size_t bytes_received = recv(sd, buffer, sizeof(buffer) - 1, 0);
    if (getIsConnected() == false)
    {
        *m_cout << __func__ << ":" << "Server is shuting down. Cannot read messages.\n"; 
        return EXIT_SUCCESS;
    }
    if (strcmp(buffer, "quit") == 0)
    {
        *m_cout << __func__ << ":" << "Quit command received. Closing connection.\n";
        return EXIT_SUCCESS;
    }
    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0'; // Null-terminate the received data
        //getClientIP(sd);
        *m_cout << __func__ << ":" << "Message size: " << bytes_received << "\n";
        *m_cout << __func__ << ":" << "Message from client: " << buffer << "\n";
        message = string(buffer);
        return bytes_received;
    }
    if (bytes_received == 0)
    {
        // Client closed the connection
        return EXIT_SUCCESS;
    }
    else
    {
        *m_cout << __func__ << ":" << "Error receiving data from client.\n";
        logErrorMessage(ERROR_CODE);;
    }
    return EXIT_FAILURE; // Indicate error or connection closed
}

size_t ServerSocket::getClientCount(){
    return m_ClientSockets.size();
}

void ServerSocket::threadBroadcastMessage() {
    *m_cout << __func__ << ":" << "Broadcast thread started.\n";

    m_BroadcastThread = jthread([this](stop_token token) {
        while (!token.stop_requested()) {
            pair<int, string> front;
            {
                lock_guard<mutex> lock(m_mutex);
                if (!m_BroadcastMessageQueue.empty()) {
                
                    front = m_BroadcastMessageQueue.front();
                    m_BroadcastMessageQueue.pop();
                }
            }
            
            if (front.first > 0) {
                int sd = front.first;
                const string& message = front.second;
                if (sendMessage(sd, message)) {
                    *m_cout << "threadBroadcastMessage:sd: " << sd << ": Sent message to client: " << message << "\n";
                } else {
                    *m_cout << "threadBroadcastMessage:Failed to send message to client: " << sd << "\n";
                }
                m_cout->log("threadBroadcastMessage:***********************************************");
            }
            this_thread::yield;
        }
        m_cout->log("threadBroadcastMessage:Broadcast thread stoped");
    });
}

void ServerSocket::closeSocket(int sd)
{
    CLOSESOCKET(sd);
    m_ClientSockets.erase(sd);
    *m_cout << __func__ << ":" << " Socket closed.\n";
}

unordered_set<int> ServerSocket::getClientSockets() const {
    return m_ClientSockets;
}

int ServerSocket::addBroadcastTextMessage(const string message, int sd) {
    if (m_IsConnected.load())
    {
        m_BroadcastMessageQueue.push(make_pair(sd, message));
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE; // Indicate failure if not connected
}

int ServerSocket::addBroadcastTextMessage(const string message) {
    for (const auto &sd : m_ClientSockets){
        if (addBroadcastTextMessage(message, sd) == EXIT_FAILURE){
            return EXIT_FAILURE;
        }
    }
    m_condVar.notify_one();
    return EXIT_SUCCESS;
}