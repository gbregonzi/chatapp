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

#include "chatServer.h"

constexpr int MAX_PORT_TRIES{10};
constexpr int MAX_QUEUE_CONNECTINON{10};
constexpr int BUFFER_SIZE{1024};

using namespace std;

ServerSocket::ServerSocket(Logger& logger, const string& serverName, const string& port): 
                          m_Logger(logger), m_ServerName(serverName), m_PortNumber(port)   
{
#ifdef _WIN32
    WSADATA d;
    if (WSAStartup(MAKEWORD(2,2), &d)) {
        m_Logger.log(LogLevel::Error, "{}:{}", __func__, "Failed to initialize Winsock!");
        exit(EXIT_FAILURE);
    }
#endif
    struct addrinfo hints{}, *ai = nullptr, *p = nullptr;
    int fdMax; // maximum file descriptor number
    char hostName[INET6_ADDRSTRLEN];
    char service[20];
    memset (hostName, 0, sizeof(hostName));
    memset (service, 0, sizeof(service));

    FD_ZERO(&m_Master);
    memset(&hints, 0, sizeof(hints));
    
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo(m_ServerName.c_str(), m_PortNumber.c_str(), &hints, &ai) != 0) {
        m_Logger.log(LogLevel::Error, "{}:{}",__func__, "Getaddrinfo failed!");
        m_Logger.log(LogLevel::Error, "{}:{}{}",__func__, "Error Code:", ERROR_CODE);
        m_Logger.log(LogLevel::Error, "{}:{}",__func__, gai_strerror(ERROR_CODE));
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
            m_Logger.log(LogLevel::Error, "{}:{}",__func__, "Setsockopt failed!");
            logErrorMessage(ERROR_CODE);
            CLOSESOCKET(m_sockfdListener);
            continue;
        }
        

        if (bind(m_sockfdListener, p->ai_addr, p->ai_addrlen) != -1) {
            break; // Success
        }
        
        m_Logger.log(LogLevel::Error, "{}:{}",__func__, "Bind failed! Retrying...");
        logErrorMessage(ERROR_CODE);
        CLOSESOCKET(m_sockfdListener);
    }
    

    if (p == nullptr) {
        m_Logger.log(LogLevel::Error, "{}:{}",__func__, "Failed to bind socket!");
        exit(EXIT_FAILURE);
    }
    
    m_Logger.log(LogLevel::Info, "{}:{}",__func__, "Server created and bound successfully.");
    
    if (getnameinfo(p->ai_addr, p->ai_addrlen, hostName, sizeof(hostName), service, sizeof(service), NI_NOFQDN|NI_NAMEREQD) != 0) {
        m_Logger.log(LogLevel::Error, "{}:{}",__func__, "getnameinfo fail!");
    }
    
    freeaddrinfo(ai); // all done with this
    if (listen(m_sockfdListener, MAX_QUEUE_CONNECTINON) < 0)
    {
        m_Logger.log(LogLevel::Error, "{}:{}",__func__, "Listen failed!");
        logErrorMessage(ERROR_CODE);
        exit(EXIT_FAILURE);
    }
    m_Logger.log(LogLevel::Info, "{}:{}{}{}{}",__func__, "Server name:", hostName, " Port:", service); 
    m_IsConnected.store(true);
    m_ThreadPool = make_unique<threadPool>(m_Threads);
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
            m_Logger.log(LogLevel::Error, "{}:{}",__func__, "Select failed!");
            setIsConnected(false);
            logErrorMessage(ERROR_CODE);
            exit(EXIT_FAILURE);
        }
        m_Logger.log(LogLevel::Debug, "{}:{}",__func__, "Select returned, processing sockets...");  
        for(int fd = 0; fd <= fdMax; fd++) {
            if (FD_ISSET(fd, &readFds)) { // we got one!!
                if (fd == m_sockfdListener) { // handle new connections
                    socklen_t clientAddrLen = sizeof(remoteAddr);
                    int  clientSocket = accept(m_sockfdListener, (struct sockaddr *)&remoteAddr, &clientAddrLen);
                    if (getIsConnected() == false)
                    {
                        m_Logger.log(LogLevel::Debug, "{}:{}",__func__, "Server is shutting down. Cannot accept new connections."); 
                        CLOSESOCKET(clientSocket);
                        break;
                    }
                    if (clientSocket < 0)
                    {
                        m_Logger.log(LogLevel::Error, "{}:{}",__func__, "Accept failed!");
                        logErrorMessage(ERROR_CODE);
                        continue;
                    }
                    
                    m_Logger.log(LogLevel::Debug, "{}:{}",__func__, "Client connected successfully.");
                    auto result = m_ClientSockets.emplace(clientSocket);
                    if (!result.second)
                    {
                        m_Logger.log(LogLevel::Error, "{}:{}",__func__, "Failed to add client socket to the set.");
                        CLOSESOCKET(clientSocket);
                        continue;
                    }   
                    FD_SET(clientSocket, &m_Master); // add to master set
                    if (clientSocket > fdMax) {    // keep track of the max
                        fdMax = clientSocket;
                    }
                    getClientIP(clientSocket);
                } else {
                    auto ft = m_ThreadPool->submit([this, &fd]() {
                        this->handleClientMessage(fd);
                    });
                    // auto ft = m_ThreadPool->submit(bind(&ServerSocket::handleClientMessage, this, fd));
                    if (!ft.valid()) {
                        m_ClientSockets.erase(fd);
                        FD_CLR(fd, &m_Master);
                        m_Logger.log(LogLevel::Error, "{}:{}",__func__, "Failed to submit task to thread pool.");
                    }
                }
            }
        }
    }
}

// handle data from a client
void ServerSocket::handleClientMessage(int fd) {
    //m_Logger.log(LogLevel::Debug, "{}:{}{}",__func__, "Thread id:", this_thread::get_id());  
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    size_t bytes_received = recv(fd, buffer, sizeof(buffer) - 1, 0);
    if (getIsConnected() == false)
    {
        m_Logger.log(LogLevel::Info, "{}:{}",__func__, "Server is shuting down. Cannot read messages."); 
        CLOSESOCKET(fd);
        FD_CLR(fd, &m_Master); // remove from master set
        m_ClientSockets.erase(fd);
        return;
    }
    
    if (bytes_received == ULLONG_MAX || bytes_received <= 0) {
        // got error or connection closed by client
        if (bytes_received == ULLONG_MAX || bytes_received == 0) {
            // connection closed
            m_Logger.log(LogLevel::Debug, "{}:{}{}{}",__func__, "Socket:", fd, " hung up");
        } else {
            m_Logger.log(LogLevel::Error, "{}:{}",__func__, "Recv from client failed!");
            logErrorMessage(ERROR_CODE);
        }
        CLOSESOCKET(fd); // bye!
        FD_CLR(fd, &m_Master); // remove from master set
        m_ClientSockets.erase(fd);
        return;
    }
    
    for(int j = 0; j < m_Master.fd_count; j++) {
        // send to everyone!
        if (FD_ISSET(m_Master.fd_array[j], &m_Master)) {
            // except the listener and ourselves
            if (m_Master.fd_array[j] != m_sockfdListener && m_Master.fd_array[j] != fd) {
                lock_guard<mutex> lock(m_mutex);
                auto result = m_BroadcastMessageQueue.emplace(make_pair(m_Master.fd_array[j], string(buffer)));
                if (!result.first) {
                    m_Logger.log(LogLevel::Error, "{}:{}",__func__, "Failed to add message to broadcast queue.");
                    continue;
                }
            }
        }
    }
}

void ServerSocket::logErrorMessage(int errorCode)
{
    m_Logger.log(LogLevel::Info, "{}:{}{}", __func__ , "Error Code:", errorCode);;
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
    m_Logger.log(LogLevel::Debug, "{}:{}",__func__, " Server socket closed.");    
}
void ServerSocket::closeAllClientSockets() 
{
    for (const auto &sd : m_ClientSockets){
        CLOSESOCKET(sd);
    }
    m_ClientSockets.clear();
    m_Logger.log(LogLevel::Debug, "{}:{}",__func__, " All client sockets closed.");    
}

bool ServerSocket::getClientIP(addrinfo* p){
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
    m_Logger.log(LogLevel::Error, "{}:{}:{}",__func__, ipVer, ipStr);
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
        m_Logger.log(LogLevel::Error, "{}:{}{}",__func__, " Getpeername failed! Error: ", errno);
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
    m_Logger.log(LogLevel::Info, "{}:{}:{}{}{}",__func__,  ipVer, ip, " Socket: ", port);
    return true;
}

ServerSocket::~ServerSocket()
{
    CLOSESOCKET(m_sockfdListener);
#ifdef _WIN32
     WSACleanup();
#endif
     m_Logger.log(LogLevel::Debug, "{}:{}",__func__, " ServerSocket class destroyed.");
     m_Logger.setDone(true);
}

bool ServerSocket::sendMessage(int sd, const string_view message)
{
    size_t bytesSent = send(sd, message.data(), message.length(), 0);
    if (bytesSent < 0)
    {
        m_Logger.log(LogLevel::Error, "{}:{}",__func__, " Send to client failed!");
        logErrorMessage(ERROR_CODE);
        return false;
    }
    return true;
}

void ServerSocket::threadBroadcastMessage() {
    m_Logger.log(LogLevel::Debug, "Broadcast thread started.");

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
                    m_Logger.log(LogLevel::Debug, "{}{}{}{}", "threadBroadcastMessage:sd: ", sd, ": Sent message to client: ", message);
                } else {
                    m_Logger.log(LogLevel::Error, "{}{}", "threadBroadcastMessage:Failed to send message to client: ", sd);
                }
                m_Logger.log(LogLevel::Debug, "threadBroadcastMessage:***********************************************");
            }
            this_thread::yield;
        }
        m_Logger.log(LogLevel::Error, "threadBroadcastMessage:Broadcast thread stoped");
    });
}

void ServerSocket::closeSocket(int sd)
{
    CLOSESOCKET(sd);
    m_ClientSockets.erase(sd);
    m_Logger.log(LogLevel::Debug, "{}:{}",__func__, " Socket closed.");
}

unordered_set<int> ServerSocket::getClientSockets() const {
    return m_ClientSockets;
}

