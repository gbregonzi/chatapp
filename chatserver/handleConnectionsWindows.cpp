#include "handleConnectionsWindows.h"

typedef int socklen_t;
#define CLOSESOCKET closesocket
#define ERROR_CODE WSAGetLastError()

struct ClientContext {
    SOCKET socket;
    OVERLAPPED overlapped;
    WSABUF wsabuf;
    char buffer[1024];

    ClientContext(SOCKET s) : socket(s) {
        ZeroMemory(&overlapped, sizeof(OVERLAPPED));
        wsabuf.buf = buffer;
        wsabuf.len = sizeof(buffer);
    }
};

HandleConnectionsWindows::HandleConnectionsWindows(Logger &logger, const std::string& serverName, const std::string& portNumber)
    : chatServer(logger, serverName, portNumber) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    m_IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
}

bool HandleConnectionsWindows::createListner(){
    struct addrinfo hints{}, *ai = nullptr, *p = nullptr;
    char hostName[INET6_ADDRSTRLEN];
    char service[20];
    memset (hostName, 0, sizeof(hostName));
    memset (service, 0, sizeof(service));

    memset(&hints, 0, sizeof(hints));
    
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    m_Logger.log(LogLevel::Info, "{}:Staring chatServer", __func__);

    if (getaddrinfo(m_ServerName.c_str(), m_PortNumber.c_str(), &hints, &ai) != 0) {
        m_Logger.log(LogLevel::Error, "{}:Getaddrinfo failed!",__func__);
        m_Logger.log(LogLevel::Error, "{}:Error Code:{}",__func__, ERROR_CODE);
        m_Logger.log(LogLevel::Error, "{}:{}",__func__, gai_strerror(ERROR_CODE));

        m_Logger.log(LogLevel::Info, "{}:Error Code:{}", __func__ , ERROR_CODE);
        exit(EXIT_FAILURE);
    }

    for (p = ai; p != nullptr; p = p->ai_next) {
        m_SockfdListener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (m_SockfdListener < 0) {
            continue;
        }
        
        char optval{1};
        int optlen{sizeof(optval)};
        if (setsockopt(m_SockfdListener, SOL_SOCKET, SO_REUSEADDR, &optval, optlen) < 0)
        {
            m_Logger.log(LogLevel::Error, "{}:Setsockopt failed!",__func__);
            m_Logger.log(LogLevel::Info, "{}:Error Code:{}", __func__, ERROR_CODE);
            CLOSESOCKET(m_SockfdListener);
            continue;
        }
        

        if (bind(m_SockfdListener, p->ai_addr, p->ai_addrlen) != -1) {
            break; // Success
        }
        
        m_Logger.log(LogLevel::Error, "{}:Bind failed! Retrying...",__func__);
        m_Logger.log(LogLevel::Info, "{}:Error Code:{}", __func__, ERROR_CODE);
        CLOSESOCKET(m_SockfdListener);
    }
    

    if (p == nullptr) {
        m_Logger.log(LogLevel::Error, "{}:Failed to bind socket!",__func__);
        return false;
    }
    m_Logger.log(LogLevel::Info, "{}:Server created and bound successfully.",__func__);
    
    if (getnameinfo(p->ai_addr, p->ai_addrlen, hostName, sizeof(hostName), service, sizeof(service), NI_NOFQDN|NI_NAMEREQD) != 0) {
        m_Logger.log(LogLevel::Error, "{}:getnameinfo fail!",__func__);
    }
    
    freeaddrinfo(ai); // all done with this
    if (listen(m_SockfdListener, MAX_QUEUE_CONNECTINON) < 0)
    {
        m_Logger.log(LogLevel::Error, "{}:Listen failed!",__func__);
        m_Logger.log(LogLevel::Info, "{}:Error Code:{}", __func__, ERROR_CODE);
        return false;
    }
    m_Logger.log(LogLevel::Info, "{}:Server name:{} Port:{}",__func__, hostName, service);
    m_Logger.log(LogLevel::Info, "{}:Using IOCP for handling connections.",__func__);
    
    CreateIoCompletionPort((HANDLE)m_SockfdListener, m_IOCP, (ULONG_PTR)m_SockfdListener, 0);
    m_Logger.log(LogLevel::Info, "{}:Server is ready to accept connections.",__func__);
    for (int i = 0; i < MAX_THREAD ; ++i) {
        m_Threads.emplace_back(&HandleConnectionsWindows::WorkerThread, this, m_IOCP);
    }
    return true; 
}

void HandleConnectionsWindows::AssociateSocket(SOCKET clientSocket) {
    ClientContext* context = new ClientContext(clientSocket);
    if (CreateIoCompletionPort((HANDLE)clientSocket, m_IOCP, (ULONG_PTR)context, 0) == NULL){
        m_Logger.log(LogLevel::Error, "{}:CreateIoCompletionPort failed:{}", __func__, ERROR_CODE);
        m_Logger.log(LogLevel::Error, "{}:{}", __func__ , getLastErrorDescription());
        closeSocket(clientSocket);
        delete context;
        return;
    }

    DWORD flags = 0;
    auto ret = WSARecv(clientSocket, &context->wsabuf, 1, NULL, &flags, &context->overlapped, NULL);
    if (ret != 0 && ERROR_CODE != WSA_IO_PENDING) {
        m_Logger.log(LogLevel::Error, "{}:WSARecv failed:{}", __func__, ERROR_CODE);
        m_Logger.log(LogLevel::Error, "{}:{}", __func__ , getLastErrorDescription());
        closeSocket(clientSocket);
        delete context;
    }
}

void HandleConnectionsWindows::WorkerThread(HANDLE iocp) {
    DWORD bytesTransferred;
    ULONG_PTR completionKey;
    LPOVERLAPPED lpOverlapped;

    while (getIsConnected()) {
        BOOL result = GetQueuedCompletionStatus(iocp, &bytesTransferred, &completionKey, &lpOverlapped, INFINITE);
        if (!result || lpOverlapped == nullptr) continue;

        ClientContext* context = reinterpret_cast<ClientContext*>(completionKey);

        if (bytesTransferred == 0) {
            
            closeSocket(context->socket);
            delete context;
            continue;
        }

        for (auto& sd:m_ClientSockets){
            if (sd != context->socket){
                lock_guard lock(m_Mutex);
                context->buffer[bytesTransferred] = 0x0;
                m_BroadcastMessageQueue.emplace(make_pair(sd, context->buffer));    
            }    
        }

        ZeroMemory(&context->overlapped, sizeof(OVERLAPPED));
        DWORD flags = 0;
        WSARecv(context->socket, &context->wsabuf, 1, NULL, &flags, &context->overlapped, NULL);
    }
}


void HandleConnectionsWindows::AcceptConnections() {
    m_Logger.log(LogLevel::Info, "{}:Accept Connections start", __func__);
    while (getIsConnected()) {
        SOCKET clientSocket = accept(m_SockfdListener, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            m_Logger.log(LogLevel::Error, "{}:Accept failed:{}", __func__, getLastErrorDescription());
            continue;
        }

        m_Logger.log(LogLevel::Debug, "{}:Client connected successfully.",__func__);
        lock_guard lock(m_Mutex);
        auto result = m_ClientSockets.emplace(clientSocket);
        if (!result.second)
        {
            m_Logger.log(LogLevel::Error, "{}:Failed to add client socket to the set.",__func__);
            m_Logger.log(LogLevel::Error, "{}:{}", __func__ , getLastErrorDescription());
            CLOSESOCKET(clientSocket);
            continue;
        }   
        getClientIP(clientSocket);
        AssociateSocket(clientSocket);
    }
    CloseHandle(m_IOCP);
}

HandleConnectionsWindows::~HandleConnectionsWindows()
{
    CLOSESOCKET(m_SockfdListener);
    setIsConnected(false);
    m_BroadcastThread.request_stop();
    WSACleanup();
    m_Logger.log(LogLevel::Debug, "{}:HandleConnectionsWindows class destroyed.",__func__);
    m_Logger.setDone(true);
}
