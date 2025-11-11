
#pragma once

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

void HandleConnectionsWindows::AssociateSocket(SOCKET clientSocket) {
    ClientContext* context = new ClientContext(clientSocket);
    if (CreateIoCompletionPort((HANDLE)clientSocket, m_IOCP, (ULONG_PTR)context, 0) == NULL){
        m_Logger.log(LogLevel::Error, "{}:CreateIoCompletionPort failed:{}", __func__, ERROR_CODE);
        m_Logger.log(LogLevel::Error, "{}:{}", __func__ , getLastErrorDescription());
        CLOSESOCKET(clientSocket);
        lock_guard<mutex> lock(m_Mutex);
        m_ClientSockets.erase(clientSocket);
        delete context;
        return;
    }

    DWORD flags = 0;
    auto ret = WSARecv(clientSocket, &context->wsabuf, 1, NULL, &flags, &context->overlapped, NULL);
    if (ret != 0 && ERROR_CODE != WSA_IO_PENDING) {
        m_Logger.log(LogLevel::Error, "{}:WSARecv failed:{}", __func__, ERROR_CODE);
        m_Logger.log(LogLevel::Error, "{}:{}", __func__ , getLastErrorDescription());
        closesocket(clientSocket);
        lock_guard<mutex> lock(m_Mutex);
        m_ClientSockets.erase(clientSocket);
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
            closesocket(context->socket);
            lock_guard<mutex> lock(m_Mutex);
            m_ClientSockets.erase(context->socket);
            delete context;
            continue;
        }

        for (auto& sd:m_ClientSockets){
            if (sd != context->socket){
                lock_guard<mutex> lock(m_Mutex);
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

    while (getIsConnected()) {
        SOCKET clientSocket = accept(m_sockfdListener, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            m_Logger.log(LogLevel::Error, "{}:Accept failed:{}", __func__, getLastErrorDescription());
            continue;
        }

        m_Logger.log(LogLevel::Debug, "{}:Client connected successfully.",__func__);
        lock_guard<mutex> lock(m_Mutex);
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
}

void HandleConnectionsWindows::handleSelectConnections(){
    m_Logger.log(LogLevel::Info, "{}:Using IOCP for handling connections.",__func__);
    m_IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    CreateIoCompletionPort((HANDLE)m_sockfdListener, m_IOCP, (ULONG_PTR)m_sockfdListener, 0);
    m_Logger.log(LogLevel::Info, "{}:Server is ready to accept connections.",__func__);
    for (int i = 0; i < MAX_THREAD ; ++i) {
        m_Threads.emplace_back(&HandleConnectionsWindows::WorkerThread, this, m_IOCP);
    }
    AcceptConnections();

    CloseHandle(m_IOCP);
}