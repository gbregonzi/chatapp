#include "handleConnectionsWindows.h"

typedef int socklen_t;
#define ERROR_CODE WSAGetLastError()

struct ClientContext {
    SOCKET socket;
    OVERLAPPED overlapped;
    WSABUF wsabuf;
    static constexpr int BUFFER_SZ = 1024;
    char buffer[BUFFER_SZ];

    ClientContext(SOCKET s) : socket(s) {
        ZeroMemory(&overlapped, sizeof(OVERLAPPED));
        wsabuf.buf = buffer;
        wsabuf.len = sizeof(buffer);
    }
};

HandleConnectionsWindows::HandleConnectionsWindows(Logger &logger, const std::string& serverName, const string& portNumber)
    : ChatServer(logger, serverName, portNumber) {
    m_Logger.log(LogLevel::Debug, "{}:HandleConnectionsWindows class created.",__func__);
    
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

void HandleConnectionsWindows::createIoCompletionPort(){
    m_Logger.log(LogLevel::Info, "{}:Creating IO Completion Port",__func__);

    m_IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
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
        lock_guard lock(m_Mutex);
        m_ClientSockets.erase(clientSocket);
        closesocket(clientSocket);
        delete context;
        return;
    }

    DWORD flags = 0;
    auto ret = WSARecv(clientSocket, &context->wsabuf, 1, NULL, &flags, &context->overlapped, NULL);
    if (ret != 0 && ERROR_CODE != WSA_IO_PENDING) {
        m_Logger.log(LogLevel::Error, "{}:WSARecv failed:{}", __func__, ERROR_CODE);
        m_Logger.log(LogLevel::Error, "{}:{}", __func__ , getLastErrorDescription());
        lock_guard lock(m_Mutex);
        m_ClientSockets.erase(clientSocket);
        closeSocket(clientSocket);
        delete context;
    }
}

void HandleConnectionsWindows::workerThread(HANDLE iocp) {
    m_Logger.log(LogLevel::Debug, "{}:Worker thread started.",__func__);
    
    DWORD bytesTransferred;
    ULONG_PTR completionKey;
    LPOVERLAPPED lpOverlapped;
    int msgLen{0};
    int totalBytesRead{0};

    while (getIsConnected()) {
        BOOL result = GetQueuedCompletionStatus(iocp, &bytesTransferred, &completionKey, &lpOverlapped, INFINITE);
        if (!result || lpOverlapped == nullptr) {
            continue;
        }

        ClientContext* context = reinterpret_cast<ClientContext*>(completionKey);

        if (bytesTransferred == 0) {
            closeSocket(context->socket);
            lock_guard lock(m_Mutex);
            m_ClientSockets.erase(context->socket);
            m_Logger.log(LogLevel::Debug, "{}:Client disconnected.",__func__);    
            delete context;
            continue;
        }
        context->buffer[bytesTransferred] = 0x0;
        totalBytesRead += bytesTransferred;
        cout << __func__ << ":Bytes received:" << bytesTransferred << "\n";
        if (msgLen == 0){
            msgLen = stoi(string(context->buffer).substr(0, 4));
        }
        if (msgLen == totalBytesRead){ 
            lock_guard lock(m_Mutex);
            cout << __func__ << ":Message to broadcast:" << string(context->buffer).substr(4, msgLen) << "\n";  
            for (auto& sd:m_ClientSockets){
                if (sd != context->socket){
                    msgLen = 0;
                    totalBytesRead = 0;
                    addProadcastMessage(sd, string(context->buffer).substr(4, msgLen));
                }    
            }
        }

        ZeroMemory(&context->overlapped, sizeof(OVERLAPPED));
        DWORD flags = 0;
        auto errorCode = WSARecv(context->socket, &context->wsabuf + totalBytesRead, 1, NULL, &flags, &context->overlapped, NULL);
        if (errorCode == SOCKET_ERROR) {
            int wserr = WSAGetLastError();
            if (wserr != WSA_IO_PENDING) {
                lock_guard lock(m_Mutex);
                m_ClientSockets.erase(context->socket);
                closesocket(context->socket);
                delete context;
                continue;
            }
        }
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
        pair<unordered_set<int>::iterator, bool> result;
        {
            lock_guard lock(m_Mutex);
            result = m_ClientSockets.emplace(clientSocket);
        }
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
    m_Logger.log(LogLevel::Debug, "{}:HandleConnectionsWindows class destroyed.",__func__);
    closesocket(m_SockfdListener);
    setIsConnected(false);
    m_BroadcastThread.request_stop();
    WSACleanup();
    m_Logger.setDone(true);
}
