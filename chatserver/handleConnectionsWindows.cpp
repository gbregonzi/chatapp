#include "handleConnectionsWindows.h"

typedef int socklen_t;
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
    : ChatServer(logger, serverName, portNumber) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    m_IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
}

void HandleConnectionsWindows::createIoCompletionPort(){
    CreateIoCompletionPort((HANDLE)m_SockfdListener, m_IOCP, (ULONG_PTR)m_SockfdListener, 0);
    m_Logger.log(LogLevel::Info, "{}:Server is ready to accept connections.",__func__);
    for (int i = 0; i < MAX_THREAD ; ++i) {
        m_Threads.emplace_back(&HandleConnectionsWindows::workerThread, this, m_IOCP);
    }
}

void HandleConnectionsWindows::associateSocket(SOCKET clientSocket) {
    ClientContext* context = new ClientContext(clientSocket);
    if (CreateIoCompletionPort((HANDLE)clientSocket, m_IOCP, (ULONG_PTR)context, 0) == NULL){
        m_Logger.log(LogLevel::Error, "{}:CreateIoCompletionPort failed:{}", __func__, ERROR_CODE);
        m_Logger.log(LogLevel::Error, "{}:{}", __func__ , getLastErrorDescription());
        closesocket(clientSocket);
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

void HandleConnectionsWindows::workerThread(HANDLE iocp) {
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


void HandleConnectionsWindows::acceptConnections() {
    createIoCompletionPort();

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
            closesocket(clientSocket);
            continue;
        }   
        getClientIP(clientSocket);
        associateSocket(clientSocket);
    }
    CloseHandle(m_IOCP);
}

HandleConnectionsWindows::~HandleConnectionsWindows()
{
    closesocket(m_SockfdListener);
    setIsConnected(false);
    m_BroadcastThread.request_stop();
    WSACleanup();
    m_Logger.log(LogLevel::Debug, "{}:HandleConnectionsWindows class destroyed.",__func__);
    m_Logger.setDone(true);
}
