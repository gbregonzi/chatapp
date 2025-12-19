#include "handleConnectionsWindows.h"

typedef int socklen_t;
#define ERROR_CODE WSAGetLastError()

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
    m_Logger.log(LogLevel::Debug, "{}:Associating socket with IOCP...", __func__);

    ClientContext* ctx = new ClientContext();
    if (CreateIoCompletionPort((HANDLE)clientSocket, m_IOCP, (ULONG_PTR)clientSocket, 0) == NULL){
        m_Logger.log(LogLevel::Error, "{}:CreateIoCompletionPort failed:{}", __func__, ERROR_CODE);
        m_Logger.log(LogLevel::Error, "{}:{}", __func__ , getLastErrorDescription());
        lock_guard lock(m_Mutex);
        m_ClientSockets.erase(clientSocket);
        closesocket(clientSocket);
        delete ctx;
        return;
    }

    ctx->state = ClientContext::READ_HEADER;
    ctx->expected = sizeof(uint32_t);
    ctx->received = 0;
    ctx->wsabuf.buf = ctx->buffer;
    ctx->wsabuf.len = ctx->expected;

    DWORD flags = 0, bytes = 0;
    auto ret = WSARecv(clientSocket, &ctx->wsabuf, 1, &bytes, &flags, &ctx->overlapped, NULL);
    if (ret != 0 && ERROR_CODE != WSA_IO_PENDING) {
        m_Logger.log(LogLevel::Error, "{}:WSARecv failed:{}", __func__, ERROR_CODE);
        m_Logger.log(LogLevel::Error, "{}:{}", __func__ , getLastErrorDescription());
        lock_guard lock(m_Mutex);
        m_ClientSockets.erase(clientSocket);
        closeSocket(clientSocket);
        delete ctx;
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
        ClientContext* ctx = reinterpret_cast<ClientContext*>(lpOverlapped);
        if (!result || bytesTransferred == 0) {
            closeSocket(completionKey);
            lock_guard lock(m_Mutex);
            m_ClientSockets.erase(completionKey);
            m_Logger.log(LogLevel::Debug, "{}:Client disconnected.",__func__);    
            delete ctx;
            continue;
        }
        // context->buffer[bytesTransferred] = 0x0;
        // totalBytesRead += bytesTransferred;
        // cout << __func__ << ":Bytes received:" << bytesTransferred << "\n";
        // cout << __func__ << ":Total bytes received so far:" << totalBytesRead << "\n";
        // if (msgLen == 0){
        //     msgLen = stoi(string(context->buffer).substr(0, 4));
        // }
        // if (msgLen == totalBytesRead){ 
        //     lock_guard lock(m_Mutex);
        //     cout << __func__ << ":Message to broadcast:" << string(context->buffer).substr(4, msgLen) << "\n";  
        //     for (auto& sd:m_ClientSockets){
        //         if (sd != context->socket){
        //             addProadcastMessage(sd, string(context->buffer).substr(4, msgLen));
        //         }    
        //     }
        //     msgLen = 0;
        //     totalBytesRead = 0;
        // }

        // ZeroMemory(&context->overlapped, sizeof(OVERLAPPED));
        // DWORD flags = 0;
        // auto errorCode = WSARecv(context->socket, &context->wsabuf + totalBytesRead, 1, NULL, &flags, &context->overlapped, NULL);
        // if (errorCode == SOCKET_ERROR) {
        //     int wserr = WSAGetLastError();
        //     if (wserr != WSA_IO_PENDING) {
        //         lock_guard lock(m_Mutex);
        //         m_ClientSockets.erase(context->socket);
        //         closesocket(context->socket);
        //         logLastError(m_Logger, wserr);
        //         delete context;
        //         continue;
        //     }
        // }
        handleCompletion(ctx, bytesTransferred, (SOCKET)completionKey);
    }
}
void HandleConnectionsWindows::repostRecv(SOCKET sd, ClientContext* ctx) {
    m_Logger.log(LogLevel::Debug, "{}:Reposting receive...", __func__);

    ctx->wsabuf.buf = ctx->buffer + ctx->received;
    ctx->wsabuf.len = ctx->expected - ctx->received;

    DWORD flags = 0, bytes = 0;
    int rc = WSARecv(sd, &ctx->wsabuf, 1, &bytes, &flags, &ctx->overlapped, NULL);
    if (rc == SOCKET_ERROR && ERROR_CODE != WSA_IO_PENDING) {
        m_Logger.log(LogLevel::Error, "{}:WSARecv failed: {}", __func__, ERROR_CODE);
    }
}

void HandleConnectionsWindows::postReadHeader(SOCKET sd, ClientContext* ctx) {
    m_Logger.log(LogLevel::Debug, "{}:Posting read for header...", __func__);

    ctx->state = ClientContext::READ_HEADER;
    ctx->expected = sizeof(uint32_t);
    ctx->received = 0;
    ctx->wsabuf.buf = ctx->buffer;
    ctx->wsabuf.len = ctx->expected;

    DWORD flags = 0, bytes = 0;
    int rc = WSARecv(sd, &ctx->wsabuf, 1, &bytes, &flags, &ctx->overlapped, NULL);
    if (rc == SOCKET_ERROR && ERROR_CODE != WSA_IO_PENDING) {
        m_Logger.log(LogLevel::Error, "{}:WSARecv failed: {}", __func__, ERROR_CODE);
    }
}

void HandleConnectionsWindows::handleCompletion(ClientContext* ctx, DWORD bytesTransferred, SOCKET sd) {
    m_Logger.log(LogLevel::Debug, "{}:Handling completion, bytes transferred: {}", __func__, bytesTransferred);

    ctx->received += bytesTransferred;

    if (ctx->state == ClientContext::READ_HEADER) {
        if (ctx->received < ctx->expected) {
            repostRecv(sd, ctx);
            return;
        }

        uint32_t msgLen;
        msgLen = atoi(ctx->buffer);

        ctx->state = ClientContext::READ_BODY;
        ctx->expected = msgLen;
        ctx->received = 0;
        ctx->wsabuf.buf = ctx->buffer;
        ctx->wsabuf.len = msgLen;

        ZeroMemory(&ctx->overlapped, sizeof(OVERLAPPED));
        DWORD flags = 0, bytes = 0;
        auto errorCode = WSARecv(sd, &ctx->wsabuf, 1, &bytes, &flags, &ctx->overlapped, NULL);
        if (errorCode == SOCKET_ERROR) {
            int wserr = WSAGetLastError();
            if (wserr != WSA_IO_PENDING) {
                lock_guard lock(m_Mutex);
                m_ClientSockets.erase(sd);
                closesocket(sd);
                logLastError(m_Logger, wserr);
            }
        }
    }
    else if (ctx->state == ClientContext::READ_BODY) {
        if (ctx->received < ctx->expected) {
            repostRecv(sd, ctx);
            return;
        }

        string message(ctx->buffer, ctx->expected);
        lock_guard lock(m_Mutex);
        for (auto& _sd:m_ClientSockets){
            if (_sd != sd){
                addProadcastMessage(_sd, message);
            }    
        }

        postReadHeader(sd, ctx); // loop back to next message
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
