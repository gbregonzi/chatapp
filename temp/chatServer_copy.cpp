#ifdef _WIN32
    typedef int socklen_t;
    #define CLOSESOCKET closesocket
    #define ERROR_CODE WSAGetLastError()
#else
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

// constexpr int MAX_PORT_TRIES{10};
// constexpr int MAX_QUEUE_CONNECTINON{10};
// constexpr int BUFFER_SIZE{1024};

using namespace std;

// #ifdef _WIN32
// struct ClientContext {
//     SOCKET socket;
//     OVERLAPPED overlapped;
//     WSABUF wsabuf;
//     char buffer[1024];

//     ClientContext(SOCKET s) : socket(s) {
//         ZeroMemory(&overlapped, sizeof(OVERLAPPED));
//         wsabuf.buf = buffer;
//         wsabuf.len = sizeof(buffer);
//     }
// };
// #endif

// void chatServer::AssociateSocket(SOCKET clientSocket) {
// #ifdef _WIN32
//     ClientContext* context = new ClientContext(clientSocket);
//     if (CreateIoCompletionPort((HANDLE)clientSocket, m_IOCP, (ULONG_PTR)context, 0) == NULL){
//         m_Logger.log(LogLevel::Error, "{}:CreateIoCompletionPort failed:{}", __func__, ERROR_CODE);
//         m_Logger.log(LogLevel::Error, "{}:{}", __func__ , getLastErrorDescription());
//         CLOSESOCKET(clientSocket);
//         lock_guard<mutex> lock(m_Mutex);
//         m_ClientSockets.erase(clientSocket);
//         delete context;
//         return;
//     }

//     DWORD flags = 0;
//     auto ret = WSARecv(clientSocket, &context->wsabuf, 1, NULL, &flags, &context->overlapped, NULL);
//     if (ret != 0 && ERROR_CODE != WSA_IO_PENDING) {
//         m_Logger.log(LogLevel::Error, "{}:WSARecv failed:{}", __func__, ERROR_CODE);
//         m_Logger.log(LogLevel::Error, "{}:{}", __func__ , getLastErrorDescription());
//         closesocket(clientSocket);
//         lock_guard<mutex> lock(m_Mutex);
//         m_ClientSockets.erase(clientSocket);
//         delete context;
//     }
// #else

// #endif
// }


chatServer::chatServer(Logger& logger, const string& serverName, const string& port): 
                          m_Logger(logger), m_ServerName(serverName), m_PortNumber(port)   
{
    bool ret = createListner();
    
    AcceptConnections();

// #ifdef _WIN32
//     WSADATA d;
//     if (WSAStartup(MAKEWORD(2,2), &d)) {
//         m_Logger.log(LogLevel::Error, "{}:Failed to initialize Winsock!", __func__);
//         exit(EXIT_FAILURE);
//     }
// #endif
//     struct addrinfo hints{}, *ai = nullptr, *p = nullptr;
//     //int fdMax; // maximum file descriptor number
//     char hostName[INET6_ADDRSTRLEN];
//     char service[20];
//     memset (hostName, 0, sizeof(hostName));
//     memset (service, 0, sizeof(service));

//     FD_ZERO(&m_Master);
//     memset(&hints, 0, sizeof(hints));
    
//     hints.ai_family = AF_UNSPEC;
//     hints.ai_socktype = SOCK_STREAM;
//     hints.ai_flags = AI_PASSIVE;
//     if (getaddrinfo(m_ServerName.c_str(), m_PortNumber.c_str(), &hints, &ai) != 0) {
//         m_Logger.log(LogLevel::Error, "{}:Getaddrinfo failed!",__func__);
//         m_Logger.log(LogLevel::Error, "{}:Error Code:{}",__func__, ERROR_CODE);
//         m_Logger.log(LogLevel::Error, "{}:{}",__func__, gai_strerror(ERROR_CODE));
        
//         m_Logger.log(LogLevel::Info, "{}:Error Code:{}", __func__ , ERROR_CODE);
//         exit(EXIT_FAILURE);
//     }
    
//     for (p = ai; p != nullptr; p = p->ai_next) {
//         m_sockfdListener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
//         if (m_sockfdListener < 0) {
//             continue;
//         }
        
//         char optval{1};
//         int optlen{sizeof(optval)};
//         if (setsockopt(m_sockfdListener, SOL_SOCKET, SO_REUSEADDR, &optval, optlen) < 0)
//         {
//             m_Logger.log(LogLevel::Error, "{}:Setsockopt failed!",__func__);
//             m_Logger.log(LogLevel::Info, "{}:Error Code:{}", __func__, ERROR_CODE);
//             CLOSESOCKET(m_sockfdListener);
//             continue;
//         }
        

//         if (bind(m_sockfdListener, p->ai_addr, p->ai_addrlen) != -1) {
//             break; // Success
//         }
        
//         m_Logger.log(LogLevel::Error, "{}:Bind failed! Retrying...",__func__);
//         m_Logger.log(LogLevel::Info, "{}:Error Code:{}", __func__, ERROR_CODE);
//         CLOSESOCKET(m_sockfdListener);
//     }
    

//     if (p == nullptr) {
//         m_Logger.log(LogLevel::Error, "{}:Failed to bind socket!",__func__);
//         exit(EXIT_FAILURE);
//     }
    
//     m_Logger.log(LogLevel::Info, "{}:Server created and bound successfully.",__func__);
    
//     if (getnameinfo(p->ai_addr, p->ai_addrlen, hostName, sizeof(hostName), service, sizeof(service), NI_NOFQDN|NI_NAMEREQD) != 0) {
//         m_Logger.log(LogLevel::Error, "{}:getnameinfo fail!",__func__);
//     }
    
//     freeaddrinfo(ai); // all done with this
//     if (listen(m_sockfdListener, MAX_QUEUE_CONNECTINON) < 0)
//     {
//         m_Logger.log(LogLevel::Error, "{}:Listen failed!",__func__);
//         m_Logger.log(LogLevel::Info, "{}:Error Code:{}", __func__, ERROR_CODE);
//         exit(EXIT_FAILURE);
//     }
//     m_Logger.log(LogLevel::Info, "{}:Server name:{} Port:{}",__func__, hostName, service); 
    m_IsConnected.store(true);
    threadBroadcastMessage();
}

string chatServer::getLastErrorDescription() {
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
    result = format("{}: {}", ERROR_CODE, errorMsg);
    LocalFree(errorMsg);
#else
    result = strerror(ERROR_CODE);
#endif
    return result;
}
// #ifdef _WIN32
// void chatServer::WorkerThread(HANDLE iocp) {
//     DWORD bytesTransferred;
//     ULONG_PTR completionKey;
//     LPOVERLAPPED lpOverlapped;

//     while (m_IsConnected.load()) {
//         BOOL result = GetQueuedCompletionStatus(iocp, &bytesTransferred, &completionKey, &lpOverlapped, INFINITE);
//         if (!result || lpOverlapped == nullptr) continue;

//         ClientContext* context = reinterpret_cast<ClientContext*>(completionKey);

//         if (bytesTransferred == 0) {
//             closesocket(context->socket);
//             lock_guard<mutex> lock(m_Mutex);
//             m_ClientSockets.erase(context->socket);
//             delete context;
//             continue;
//         }

//         for (auto& sd:m_ClientSockets){
//             if (sd != context->socket){
//                 lock_guard<mutex> lock(m_Mutex);
//                 context->buffer[bytesTransferred] = 0x0;
//                 m_BroadcastMessageQueue.emplace(make_pair(sd, context->buffer));    
//             }    
//         }

//         ZeroMemory(&context->overlapped, sizeof(OVERLAPPED));
//         DWORD flags = 0;
//         WSARecv(context->socket, &context->wsabuf, 1, NULL, &flags, &context->overlapped, NULL);
//     }
// }

// void chatServer::AcceptConnections() {

//     while (m_IsConnected.load()) {
//         SOCKET clientSocket = accept(m_sockfdListener, nullptr, nullptr);
//         if (clientSocket == INVALID_SOCKET) {
//             m_Logger.log(LogLevel::Error, "{}:Accept failed:{}", __func__, getLastErrorDescription());
//             continue;
//         }

//         m_Logger.log(LogLevel::Debug, "{}:Client connected successfully.",__func__);
//         lock_guard<mutex> lock(m_Mutex);
//         auto result = m_ClientSockets.emplace(clientSocket);
//         if (!result.second)
//         {
//             m_Logger.log(LogLevel::Error, "{}:Failed to add client socket to the set.",__func__);
//             m_Logger.log(LogLevel::Error, "{}:{}", __func__ , getLastErrorDescription());
//             CLOSESOCKET(clientSocket);
//             continue;
//         }   
//         getClientIP(clientSocket);
//         AssociateSocket(clientSocket);
//     }
// }
// #endif

// void chatServer::handleSelectConnections(){
// #ifdef _WIN32
//     m_Logger.log(LogLevel::Info, "{}:Using IOCP for handling connections.",__func__);
//     m_IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
//     CreateIoCompletionPort((HANDLE)m_sockfdListener, m_IOCP, (ULONG_PTR)m_sockfdListener, 0);
//     m_Logger.log(LogLevel::Info, "{}:Server is ready to accept connections.",__func__);
//     for (int i = 0; i < MAX_THREAD ; ++i) {
//         m_Threads.emplace_back(&chatServer::WorkerThread, this, m_IOCP);
//     }
//     AcceptConnections();

//     CloseHandle(m_IOCP);
// #else
//     int epfd = epoll_create1(0);
//     epoll_event ev{.events = EPOLLIN, .data = {.fd = m_sockfdListener}};
//     epoll_ctl(epfd, EPOLL_CTL_ADD, m_sockfdListener, &ev);
//     while(getIsConnected()){
//         epoll_event events[MAX_QUEUE_CONNECTINON];
//         int nfds = epoll_wait(epfd, events, MAX_QUEUE_CONNECTINON, -1);
//         for (int i = 0; i < nfds; ++i) {
//             if (events[i].data.fd == m_sockfdListener) {
//                 int client_fd = accept(m_sockfdListener, nullptr, nullptr);
//                 set_nonblocking(client_fd);
//                 handle_client(client_fd, epfd);
//             } else {
//                 auto h = coroutine_handle<>::from_address(events[i].data.ptr);
//                 epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, nullptr);
//                 h.resume();
//             }
//         }

//     }
//     // struct sockaddr_storage remoteAddr; // client address
//     // fd_set readFds; // temp file descriptor list for select()
//     // int fdMax; // maximum file descriptor number
//     // FD_ZERO(&readFds);
    
//     // FD_SET(m_sockfdListener, &m_Master);
//     // fdMax = m_sockfdListener; 
//     // m_Logger.log(LogLevel::Debug, "{}:m_sockfdListener:{}",__func__, m_sockfdListener);  
        
//     // while(getIsConnected()) {
//     //     {
//     //         lock_guard<mutex> lock(m_Mutex);
//     //         readFds = m_Master; // copy it
//     //     }
//     //     struct timeval tv;
//     //     tv.tv_sec = 2;
//     //     tv.tv_usec = 500000; // 500 milliseconds
        
//     //     if (select(fdMax+1, &readFds, nullptr, nullptr, &tv) == -1) {
//     //         m_Logger.log(LogLevel::Error, "{}:Select failed!",__func__);
//     //         setIsConnected(false);
//     //         m_Logger.log(LogLevel::Info, "{}:Error Code:{}", __func__ , ERROR_CODE);
//     //         exit(EXIT_FAILURE);
//     //     }
//     //     //m_Logger.log(LogLevel::Debug, "{}:Select returned, processing sockets...",__func__);  
//     //     for(int fd = 0; fd <= fdMax; fd++) {
//     //         if (FD_ISSET(fd, &readFds)) { 
//     //             if (fd == m_sockfdListener) { // handle new connections
//     //                 socklen_t clientAddrLen = sizeof(remoteAddr);
//     //                 int  clientSocket = accept(m_sockfdListener, (struct sockaddr *)&remoteAddr, &clientAddrLen);
//     //                 if (getIsConnected() == false)
//     //                 {
//     //                     m_Logger.log(LogLevel::Debug, "{}:Server is shutting down. Cannot accept new connections.",__func__); 
//     //                     CLOSESOCKET(clientSocket);
//     //                     break;
//     //                 }
//     //                 if (clientSocket < 0)
//     //                 {
//     //                     m_Logger.log(LogLevel::Error, "{}:Accept failed!",__func__);
//     //                     m_Logger.log(LogLevel::Info, "{}:Error Code:{}", __func__ , ERROR_CODE);
//     //                     continue;
//     //                 }
                    
//     //                 m_Logger.log(LogLevel::Debug, "{}:Client connected successfully.",__func__);
//     //                 auto result = m_ClientSockets.emplace(clientSocket);
//     //                 if (!result.second)
//     //                 {
//     //                     m_Logger.log(LogLevel::Error, "{}:Failed to add client socket to the set.",__func__);
//     //                     CLOSESOCKET(clientSocket);
//     //                     continue;
//     //                 }   
//     //                 lock_guard<mutex> lock(m_Mutex);
//     //                 FD_SET(clientSocket, &m_Master); // add to master set
//     //                 if (clientSocket > fdMax) {    // keep track of the max
//     //                     fdMax = clientSocket;
//     //                 }
//     //                 getClientIP(clientSocket);
//     //             } else {
//     //                 // auto ft = m_ThreadPool->submit([this, fd]() {
//     //                     //     this->handleClientMessage(fd);
//     //                     // });
//     //                     m_Logger.log(LogLevel::Debug, "Starting handleClientMessage fd:{}", fd);
//     //                     auto ft = m_ThreadPool->submit(bind(&chatServer::handleClientMessage, this, fd));
//     //                     // if (!ft.valid()) {
//     //                         //     m_ClientSockets.erase(fd);
//     //                         //     FD_CLR(fd, &m_Master);
//     //                         //     m_Logger.log(LogLevel::Error, "{}:Failed to submit task to thread pool.",__func__);
//     //                         // }
//     //                     }
//     //                 }
//     //                 else {
//     //                     m_Logger.log(LogLevel::Debug, "{}:Server still runnig");
//     //                 }
//     //             }
//     //         }
//     //     }
// #endif
// }
        
// handle data from a client
// void chatServer::handleClientMessage(int fd) {
//     ostringstream  threadId;
//     threadId << this_thread::get_id();
//     m_Logger.log(LogLevel::Debug, "{}:Thread id:{}",__func__, threadId.str());  
//     char buffer[BUFFER_SIZE];
//     memset(buffer, 0, sizeof(buffer));
//     size_t bytes_received = recv(fd, buffer, sizeof(buffer) - 1, 0);
//     if (getIsConnected() == false)
//     {
//         m_Logger.log(LogLevel::Info, "{}:Server is shuting down. Cannot read messages.",__func__); 
//         CLOSESOCKET(fd);
//         lock_guard<mutex> lock(m_Mutex);
//         m_ClientSockets.erase(fd);
//         FD_CLR(fd, &m_Master); // remove from master set
//         return;
//     }
    
//     if (bytes_received == ULLONG_MAX || bytes_received <= 0) {
//         // got error or connection closed by client
//         if (bytes_received == ULLONG_MAX || bytes_received == 0) {
//             m_Logger.log(LogLevel::Debug, "{}:Socket:{} hung up",__func__, fd);
//         } else {
//             m_Logger.log(LogLevel::Error, "{}:Recv from client failed!",__func__);
//             m_Logger.log(LogLevel::Info, "{}:Error Code:{}", __func__, ERROR_CODE);
//         }
//         CLOSESOCKET(fd); 
//         lock_guard<mutex> lock(m_Mutex);
//         m_ClientSockets.erase(fd);
//         FD_CLR(fd, &m_Master); // remove from master set
//         return;
//     }
    
//     lock_guard<mutex> lock(m_Mutex);
//     for (auto &cfd : m_ClientSockets) {
//         if (cfd != fd && cfd != m_sockfdListener){
//             auto result = m_BroadcastMessageQueue.emplace(make_pair(cfd, string(buffer)));
//             if (!result.first) {
//                 m_Logger.log(LogLevel::Error, "{}:Failed to add message to broadcast queue.",__func__);
//                 continue;
//             }
//         }
//     }
// }

bool chatServer::getIsConnected() const
{
    return m_IsConnected.load();
}

void chatServer::setIsConnected(bool isConnected) 
{
    m_IsConnected.store(isConnected);
}

void chatServer::closeSocketServer() 
{
    m_Logger.log(LogLevel::Debug, "{}:Server socket closed.",__func__);    
    CLOSESOCKET(m_sockfdListener);
    m_BroadcastThread.request_stop();
}
void chatServer::closeAllClientSockets() 
{
    lock_guard<mutex> lock(m_Mutex);
    for (const auto &sd : m_ClientSockets){
        CLOSESOCKET(sd);
    }
    m_ClientSockets.clear();
    m_Logger.log(LogLevel::Debug, "{}:All client sockets closed.",__func__);    
}

bool chatServer::getClientIP(int sd )
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

chatServer::~chatServer()
{
    CLOSESOCKET(m_sockfdListener);
#ifdef _WIN32
     WSACleanup();
#endif
     m_Logger.log(LogLevel::Debug, "{}:chatServer class destroyed.",__func__);
     m_Logger.setDone(true);
}

bool chatServer::sendMessage(int sd, const string_view message)
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

void chatServer::threadBroadcastMessage() {
    m_Logger.log(LogLevel::Debug, "{}:Broadcast thread started.", __func__);

    m_BroadcastThread = jthread([this](stop_token token) {
        while (!token.stop_requested()) {
            pair<int, string> front;
            {
                lock_guard<mutex> lock(m_Mutex);
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

void chatServer::closeSocket(int sd)
{
    CLOSESOCKET(sd);
    lock_guard<mutex> lock(m_Mutex);
    m_ClientSockets.erase(sd);
    m_Logger.log(LogLevel::Debug, "{}:Socket closed.",__func__);
}

unordered_set<int> chatServer::getClientSockets() const {
    return m_ClientSockets;
}

