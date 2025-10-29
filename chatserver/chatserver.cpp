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
        m_Logger.log(LogLevel::Error, "{}:Failed to initialize Winsock!", __func__);
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
        m_Logger.log(LogLevel::Error, "{}:Getaddrinfo failed!",__func__);
        m_Logger.log(LogLevel::Error, "{}:Error Code:{}",__func__, ERROR_CODE);
        m_Logger.log(LogLevel::Error, "{}:{}",__func__, gai_strerror(ERROR_CODE));
        
        m_Logger.log(LogLevel::Info, "{}:Error Code:{}", __func__ , ERROR_CODE);
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
            m_Logger.log(LogLevel::Error, "{}:Setsockopt failed!",__func__);
            m_Logger.log(LogLevel::Info, "{}:Error Code:{}", __func__, ERROR_CODE);
            CLOSESOCKET(m_sockfdListener);
            continue;
        }
        

        if (bind(m_sockfdListener, p->ai_addr, p->ai_addrlen) != -1) {
            break; // Success
        }
        
        m_Logger.log(LogLevel::Error, "{}:Bind failed! Retrying...",__func__);
        m_Logger.log(LogLevel::Info, "{}:Error Code:{}", __func__, ERROR_CODE);
        CLOSESOCKET(m_sockfdListener);
    }
    

    if (p == nullptr) {
        m_Logger.log(LogLevel::Error, "{}:Failed to bind socket!",__func__);
        exit(EXIT_FAILURE);
    }
    
    m_Logger.log(LogLevel::Info, "{}:Server created and bound successfully.",__func__);
    
    if (getnameinfo(p->ai_addr, p->ai_addrlen, hostName, sizeof(hostName), service, sizeof(service), NI_NOFQDN|NI_NAMEREQD) != 0) {
        m_Logger.log(LogLevel::Error, "{}:getnameinfo fail!",__func__);
    }
    
    freeaddrinfo(ai); // all done with this
    if (listen(m_sockfdListener, MAX_QUEUE_CONNECTINON) < 0)
    {
        m_Logger.log(LogLevel::Error, "{}:Listen failed!",__func__);
        m_Logger.log(LogLevel::Info, "{}:Error Code:{}", __func__, ERROR_CODE);
        exit(EXIT_FAILURE);
    }
    m_Logger.log(LogLevel::Info, "{}:Server name:{} Port:{}",__func__, hostName, service); 
    m_IsConnected.store(true);
    m_ThreadPool = make_unique<threadPool>(m_Threads);
    threadBroadcastMessage();
}

string ServerSocket::getLastErrorDescription() {
    string result;
    int errorCode = ERROR_CODE;
#ifdef _WIN32
    char *errorMsg = NULL;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&errorMsg,
        0,
        NULL
    );
    if (!errorMsg) {
        return format("WSA Error {}: <unknown>", errorCode);;
    }
    result = format("Error {}: {}", ERROR_CODE, errorMsg);
    LocalFree(errorMsg);
#else
    result = strerror(ERROR_CODE);
#endif
    return result;
}
struct ClientContext {
    SOCKET socket;
    OVERLAPPED overlapped;
    WSABUF wsabuf;
    char buffer[BUFFER_SIZE];
};
void WorkerThread(HANDLE iocp) {
    while (true) {
        DWORD bytesTransferred;
        ClientContext* clientContext;
        clientContext->wsabuf.buf = clientContext->buffer;
        clientContext->wsabuf.len = BUFFER_SIZE;
        ULONG_PTR key;

        // Wait for an I/O operation to complete
        if (GetQueuedCompletionStatus(iocp, &bytesTransferred, &key, (LPOVERLAPPED*)&clientContext, INFINITE)) {
            if (bytesTransferred > 0) {
                // Process the received data
                cout << "Received: " << string(clientContext->wsabuf.buf, bytesTransferred) << std::endl;

                // Echo the data back to the client
                WSASend(clientContext->socket, &clientContext->wsabuf, 1, nullptr, 0, &clientContext->overlapped, nullptr);
            } else {
                // Handle client disconnection
                closesocket(clientContext->socket);
                delete clientContext;
            }
        }
    }
}

void ServerSocket::AcceptConnections(SOCKET listenSocket, HANDLE iocp) {
    
    while (m_IsConnected.load()) {
        SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            m_Logger.log(LogLevel::Error, "{}:Accept failed:{}", __func__, ERROR_CODE);
            m_Logger.log(LogLevel::Error, "{}:{}", __func__ , getLastErrorDescription());
            continue;
        }

        // Create a new client context
        ClientContext* clientContext = new ClientContext();
        clientContext->wsabuf.buf = clientContext->buffer;
        clientContext->wsabuf.len = BUFFER_SIZE;
        clientContext->socket = clientSocket;

        m_Logger.log(LogLevel::Debug, "{}:Client connected successfully.",__func__);
        auto result = m_ClientSockets.emplace(clientSocket);
        if (!result.second)
        {
            m_Logger.log(LogLevel::Error, "{}:Failed to add client socket to the set.",__func__);
            CLOSESOCKET(clientSocket);
            continue;
        }   
        getClientIP(clientSocket);

        // Associate the socket with the IOCP
        if (CreateIoCompletionPort((HANDLE)clientSocket, iocp, (ULONG_PTR)clientContext, 0) == NULL) {
            m_Logger.log(LogLevel::Error, "{}:CreateIoCompletionPort failed:{}", __func__, ERROR_CODE);
            m_Logger.log(LogLevel::Error, "{}:{}", __func__ , getLastErrorDescription());
            CLOSESOCKET(clientSocket);
            m_ClientSockets.erase(clientSocket);
            delete clientContext;
            continue;
        }

        // Post an initial receive operation
        DWORD flags = 0;
        auto ret = WSARecv(clientSocket, &clientContext->wsabuf, BUFFER_SIZE, nullptr, &flags, &clientContext->overlapped, nullptr);
        if (ret != 0 && ERROR_CODE != WSA_IO_PENDING) {
            m_Logger.log(LogLevel::Error, "{}:WSARecv failed:{}", __func__, ERROR_CODE);
            m_Logger.log(LogLevel::Error, "{}:{}", __func__ , getLastErrorDescription());
            closesocket(clientSocket);
            m_ClientSockets.erase(clientSocket);
            delete clientContext;
            continue;
        }
    }
}

#ifdef _WIN32
void ServerSocket::handleSelectConnectionsWindows(){
    m_Logger.log(LogLevel::Debug, "{}:Using IOCP for handling connections.",__func__);
    typedef struct {
        OVERLAPPED overlapped;
        WSABUF buffer;
        char data[1024];
        SOCKET socket;
    } PER_IO_DATA;
    DWORD bytesTransferred;
    ULONG_PTR completionKey;
    LPOVERLAPPED lpOverlapped;
    DWORD flags = 0;

    HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    CreateIoCompletionPort((HANDLE)m_sockfdListener, iocp, (ULONG_PTR)m_sockfdListener, 0);
    m_Logger.log(LogLevel::Debug, "{}:Server is ready to accept connections.",__func__);
    vector<jthread> workerThreads;
    for (int i = 0; i < thread::hardware_concurrency(); ++i) {
        workerThreads.emplace_back(WorkerThread, iocp);
    }
    setIsConnected(true);
    AcceptConnections(m_sockfdListener, iocp);
    
    // while (m_IsConnected.load()) {
    //     SOCKET clientSocket = accept(m_sockfdListener, NULL, NULL);
    //     CreateIoCompletionPort((HANDLE)clientSocket, iocp, (ULONG_PTR)clientSocket, 0);

    //     PER_IO_DATA* ioData = (PER_IO_DATA*)malloc(sizeof(PER_IO_DATA));
    //     ZeroMemory(&ioData->overlapped, sizeof(OVERLAPPED));
    //     ioData->buffer.buf = ioData->data;
    //     ioData->buffer.len = sizeof(ioData->data);
    //     ioData->socket = clientSocket;

    //     auto ret = WSARecv(clientSocket, &ioData->buffer, 1, NULL, &flags, &ioData->overlapped, NULL);      
    //     if (ret =! WN_SUCCESS && ERROR_CODE != WSA_IO_PENDING) {
    //         m_Logger.log(LogLevel::Error, "{}:WSARecv failed!",__func__);
    //         m_Logger.log(LogLevel::Error, "{}:Error Code:{}:{}", __func__ , ERROR_CODE, getLastErrorDescription());
    //         CLOSESOCKET(clientSocket);
    //         free(ioData);
    //         continue;
    //     }   
        
    //     BOOL result = GetQueuedCompletionStatus(iocp, &bytesTransferred, &completionKey, &lpOverlapped, INFINITE);
    //     if (result && lpOverlapped != NULL) {
    //         PER_IO_DATA* completedData = (PER_IO_DATA*)lpOverlapped;
    //         completedData->data[bytesTransferred] = '\0'; // Null-terminate the received data   
    //         m_Logger.log(LogLevel::Debug, "Received {} bytes: {}", bytesTransferred, completedData->data);

    //         // Echo back the data
    //         send(completedData->socket, completedData->data, bytesTransferred, 0);
    //         free(completedData);
    //     }

    //     //m_Logger.log(LogLevel::Debug, "Starting handleClientMessage fd:{}", clientSocket);
    //     //auto ft = m_ThreadPool->submit(bind(&ServerSocket::handleClientMessage, this, clientSocket));
    // }
    CloseHandle(iocp);
}
#else
void ServerSocket::handleSelectConnections() {
    struct sockaddr_storage remoteAddr; // client address
    fd_set readFds; // temp file descriptor list for select()
    int fdMax; // maximum file descriptor number
    FD_ZERO(&readFds);
    
    FD_SET(m_sockfdListener, &m_Master);
    fdMax = m_sockfdListener; 
    m_Logger.log(LogLevel::Debug, "{}:m_sockfdListener:{}",__func__, m_sockfdListener);  
    
    setIsConnected(true);
    
    while(getIsConnected()) {
        {
            lock_guard<mutex> lock(m_mutex);
            readFds = m_Master; // copy it
        }
        struct timeval tv;
        tv.tv_sec = 2;
        tv.tv_usec = 500000; // 500 milliseconds
        
        if (select(fdMax+1, &readFds, nullptr, nullptr, &tv) == -1) {
            m_Logger.log(LogLevel::Error, "{}:Select failed!",__func__);
            setIsConnected(false);
            m_Logger.log(LogLevel::Info, "{}:Error Code:{}", __func__ , ERROR_CODE);
            exit(EXIT_FAILURE);
        }
        //m_Logger.log(LogLevel::Debug, "{}:Select returned, processing sockets...",__func__);  
        for(int fd = 0; fd <= fdMax; fd++) {
            if (FD_ISSET(fd, &readFds)) { 
                if (fd == m_sockfdListener) { // handle new connections
                    socklen_t clientAddrLen = sizeof(remoteAddr);
                    int  clientSocket = accept(m_sockfdListener, (struct sockaddr *)&remoteAddr, &clientAddrLen);
                    if (getIsConnected() == false)
                    {
                        m_Logger.log(LogLevel::Debug, "{}:Server is shutting down. Cannot accept new connections.",__func__); 
                        CLOSESOCKET(clientSocket);
                        break;
                    }
                    if (clientSocket < 0)
                    {
                        m_Logger.log(LogLevel::Error, "{}:Accept failed!",__func__);
                        m_Logger.log(LogLevel::Info, "{}:Error Code:{}", __func__ , ERROR_CODE);
                        continue;
                    }
                    
                    m_Logger.log(LogLevel::Debug, "{}:Client connected successfully.",__func__);
                    auto result = m_ClientSockets.emplace(clientSocket);
                    if (!result.second)
                    {
                        m_Logger.log(LogLevel::Error, "{}:Failed to add client socket to the set.",__func__);
                        CLOSESOCKET(clientSocket);
                        continue;
                    }   
                    lock_guard<mutex> lock(m_mutex);
                    FD_SET(clientSocket, &m_Master); // add to master set
                    if (clientSocket > fdMax) {    // keep track of the max
                        fdMax = clientSocket;
                    }
                    getClientIP(clientSocket);
                } else {
                    // auto ft = m_ThreadPool->submit([this, fd]() {
                        //     this->handleClientMessage(fd);
                        // });
                        m_Logger.log(LogLevel::Debug, "Starting handleClientMessage fd:{}", fd);
                        auto ft = m_ThreadPool->submit(bind(&ServerSocket::handleClientMessage, this, fd));
                        // if (!ft.valid()) {
                            //     m_ClientSockets.erase(fd);
                            //     FD_CLR(fd, &m_Master);
                            //     m_Logger.log(LogLevel::Error, "{}:Failed to submit task to thread pool.",__func__);
                            // }
                        }
                    }
                    else {
                        m_Logger.log(LogLevel::Debug, "{}:Server still runnig");
                    }
                }
            }
        }
#endif
        
// handle data from a client
void ServerSocket::handleClientMessage(int fd) {
    ostringstream  threadId;
    threadId << this_thread::get_id();
    m_Logger.log(LogLevel::Debug, "{}:Thread id:{}",__func__, threadId.str());  
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    size_t bytes_received = recv(fd, buffer, sizeof(buffer) - 1, 0);
    if (getIsConnected() == false)
    {
        m_Logger.log(LogLevel::Info, "{}:Server is shuting down. Cannot read messages.",__func__); 
        CLOSESOCKET(fd);
        m_ClientSockets.erase(fd);
        lock_guard<mutex> lock(m_mutex);
        FD_CLR(fd, &m_Master); // remove from master set
        return;
    }
    
    if (bytes_received == ULLONG_MAX || bytes_received <= 0) {
        // got error or connection closed by client
        if (bytes_received == ULLONG_MAX || bytes_received == 0) {
            m_Logger.log(LogLevel::Debug, "{}:Socket:{} hung up",__func__, fd);
        } else {
            m_Logger.log(LogLevel::Error, "{}:Recv from client failed!",__func__);
            m_Logger.log(LogLevel::Info, "{}:Error Code:{}", __func__, ERROR_CODE);
        }
        CLOSESOCKET(fd); 
        m_ClientSockets.erase(fd);
        lock_guard<mutex> lock(m_mutex);
        FD_CLR(fd, &m_Master); // remove from master set
        return;
    }
    
    lock_guard<mutex> lock(m_mutex);
    for (auto &cfd : m_ClientSockets) {
        if (cfd != fd && cfd != m_sockfdListener){
            auto result = m_BroadcastMessageQueue.emplace(make_pair(cfd, string(buffer)));
            if (!result.first) {
                m_Logger.log(LogLevel::Error, "{}:Failed to add message to broadcast queue.",__func__);
                continue;
            }
        }
    }

    // for(int j = 0; j < m_Master.fd_count; j++) {
    //     // send to everyone!
    //     if (FD_ISSET(m_Master.fd_array[j], &m_Master)) {
    //         // except the listener and ourselves
    //         if (m_Master.fd_array[j] != m_sockfdListener && m_Master.fd_array[j] != fd) {
    //             auto result = m_BroadcastMessageQueue.emplace(make_pair(m_Master.fd_array[j], string(buffer)));
    //             if (!result.first) {
    //                 m_Logger.log(LogLevel::Error, "{}:Failed to add message to broadcast queue.",__func__);
    //                 continue;
    //             }
    //         }
    //     }
    // }
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
    m_Logger.log(LogLevel::Debug, "{}:Server socket closed.",__func__);    
    CLOSESOCKET(m_sockfdListener);
    m_BroadcastThread.request_stop();
}
void ServerSocket::closeAllClientSockets() 
{
    for (const auto &sd : m_ClientSockets){
        CLOSESOCKET(sd);
    }
    m_ClientSockets.clear();
    m_Logger.log(LogLevel::Debug, "{}:All client sockets closed.",__func__);    
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
        m_Logger.log(LogLevel::Error, "{}:Getpeername failed! Error: {}",__func__, errno);
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
    m_Logger.log(LogLevel::Info, "{}:{}:{} Socket: {}",__func__,  ipVer, ip, port);
    return true;
}

ServerSocket::~ServerSocket()
{
    CLOSESOCKET(m_sockfdListener);
#ifdef _WIN32
     WSACleanup();
#endif
     m_Logger.log(LogLevel::Debug, "{}:ServerSocket class destroyed.",__func__);
     m_Logger.setDone(true);
}

bool ServerSocket::sendMessage(int sd, const string_view message)
{
    size_t bytesSent = send(sd, message.data(), message.length(), 0);
    if (bytesSent < 0)
    {
        m_Logger.log(LogLevel::Error, "{}:Send to client failed!",__func__);
        m_Logger.log(LogLevel::Info, "{}:Error Code:{}", __func__, ERROR_CODE);
        return false;
    }
    return true;
}

void ServerSocket::threadBroadcastMessage() {
    m_Logger.log(LogLevel::Debug, "{}:Broadcast thread started.", __func__);

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
                    m_Logger.log(LogLevel::Debug, "threadBroadcastMessage:sd:{} Sent message to client:{}",sd, message);
                } else {
                    m_Logger.log(LogLevel::Error, "threadBroadcastMessage:Failed to send message to client:{}", sd);
                }
                m_Logger.log(LogLevel::Debug, "threadBroadcastMessage:***********************************************");
            }
            this_thread::yield;
        }
        m_Logger.log(LogLevel::Debug, "threadBroadcastMessage:Broadcast thread stoped");
    });
}

void ServerSocket::closeSocket(int sd)
{
    CLOSESOCKET(sd);
    m_ClientSockets.erase(sd);
    m_Logger.log(LogLevel::Debug, "{}:Socket closed.",__func__);
}

unordered_set<int> ServerSocket::getClientSockets() const {
    return m_ClientSockets;
}

