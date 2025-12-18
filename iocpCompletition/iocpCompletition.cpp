#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <mutex>

#pragma comment(lib, "Ws2_32.lib")

struct IOContext {
    OVERLAPPED overlapped{};
    WSABUF buffer{};
    char data[1024];
    size_t expected = 0;
    size_t received = 0;
    enum State { READ_HEADER, READ_BODY } state = READ_HEADER;
};

using namespace std;

HANDLE hIOCP;
mutex coutMutex;
int getDataFromCompletition();

void repostRecv(SOCKET s, IOContext* ctx) {
    lock_guard lock(coutMutex);
    std::cout << __func__ << ":Reposting receive..." << std::endl;

    ctx->buffer.buf = ctx->data + ctx->received;
    ctx->buffer.len = ctx->expected - ctx->received;

    DWORD flags = 0, bytes = 0;
    int rc = WSARecv(s, &ctx->buffer, 1, &bytes, &flags, &ctx->overlapped, NULL);
    if (rc == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        std::cerr << __func__<< ":WSARecv failed: " << WSAGetLastError() << std::endl;
    }
}

void postReadHeader(SOCKET s, IOContext* ctx) {
    {
        lock_guard lock(coutMutex);
        std::cout << __func__ << ":Posting read for header..." << std::endl;
    }

    ctx->state = IOContext::READ_HEADER;
    ctx->expected = sizeof(uint32_t);
    ctx->received = 0;
    ctx->buffer.buf = ctx->data;
    ctx->buffer.len = ctx->expected;

    DWORD flags = 0, bytes = 0;
    int rc = WSARecv(s, &ctx->buffer, 1, &bytes, &flags, &ctx->overlapped, NULL);
    if (rc == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        std::cerr << __func__<< ":WSARecv failed: " << WSAGetLastError() << std::endl;
    }
    //getDataFromCompletition();
}

void processMessage(const std::string& msg, SOCKET sd) {
    lock_guard lock(coutMutex);
    std::cout << __func__ << ":Received message: " << msg << std::endl;

    // Echo back   
    cout << __func__ << ":Sending HEADER message." << std::endl;
    string length_str = to_string(msg.length());
    string padded_length_str = string(4 - length_str.length(), '0') + length_str;
    size_t bytesSent = send(sd, padded_length_str.data(), padded_length_str.length(), 0);
    if (bytesSent < 0)
    {
        cout << __func__ << ":Failed to send message size back to client." << std::endl;
        return;
    }

    cout << __func__ << ":Sending BODY message." << std::endl;
    bytesSent = send(sd, msg.data(), msg.length(), 0);
    if (bytesSent < 0)
    {
        cout << __func__ << ":Failed to send message back to client." << std::endl;
    }
}

void handleCompletion(IOContext* ctx, DWORD bytesTransferred, SOCKET s) {
    {
        lock_guard lock(coutMutex);
        cout << __func__ << ":Handling completion, bytes transferred: " << bytesTransferred << std::endl;
    }

    ctx->received += bytesTransferred;

    if (ctx->state == IOContext::READ_HEADER) {
        if (ctx->received < ctx->expected) {
            repostRecv(s, ctx);
            return;
        }

        uint32_t msgLen;
        memcpy(&msgLen, ctx->data, sizeof(uint32_t));

        ctx->state = IOContext::READ_BODY;
        ctx->expected = msgLen;
        ctx->received = 0;
        ctx->buffer.buf = ctx->data;
        ctx->buffer.len = msgLen;

        DWORD flags = 0, bytes = 0;
        int rc = WSARecv(s, &ctx->buffer, 1, &bytes, &flags, &ctx->overlapped, NULL);
        if (rc == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
            std::cerr << __func__ << ":WSARecv failed: " << WSAGetLastError() << std::endl;
        }
        //getDataFromCompletition();
    }
    else if (ctx->state == IOContext::READ_BODY) {
        if (ctx->received < ctx->expected) {
            repostRecv(s, ctx);
            return;
        }

        std::string message(ctx->data, ctx->expected);
        processMessage(message, s);

        postReadHeader(s, ctx); // loop back to next message
    }
}

int getDataFromCompletition() {
    {
        lock_guard lock(coutMutex);
        std::cout << __func__ << ":Waiting for completion..." << std::endl;
    }
    
    DWORD bytesTransferred;
    ULONG_PTR completionKey;
    OVERLAPPED* pOv;

    bytesTransferred = 0;
    BOOL ok = GetQueuedCompletionStatus(hIOCP, &bytesTransferred, &completionKey, &pOv, INFINITE);
    SOCKET s = (SOCKET)completionKey;
    IOContext* ctx = reinterpret_cast<IOContext*>(pOv);

    if (!ok || bytesTransferred == 0) {
        std::cerr << __func__ << ":Connection closed or error" << std::endl;
        closesocket(s);
        delete ctx;
        return -1;
    }

    handleCompletion(ctx, bytesTransferred, s);
    return 0;
}

DWORD WINAPI WorkerThread(LPVOID iocpHandle) {
    {
        lock_guard lock(coutMutex);
        std::cout << __func__ << ":Worker thread started." << std::endl;
    }
    
    HANDLE hIOCP = (HANDLE)iocpHandle;
    DWORD bytesTransferred;
    ULONG_PTR completionKey;
    OVERLAPPED* pOv;

    while (true) {
        bytesTransferred = 0;
        BOOL ok = GetQueuedCompletionStatus(hIOCP, &bytesTransferred, &completionKey, &pOv, INFINITE);
        SOCKET s = (SOCKET)completionKey;
        IOContext* ctx = reinterpret_cast<IOContext*>(pOv);

        if (!ok || bytesTransferred == 0) {
            std::cerr << __func__ << ":Connection closed or error" << std::endl;
            closesocket(s);
            delete ctx;
            continue;
        }

        handleCompletion(ctx, bytesTransferred, s);
    }
    return 0;
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9000);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(listenSock, (sockaddr*)&addr, sizeof(addr));
    listen(listenSock, SOMAXCONN);

    hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    // Worker threads
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    for (DWORD i = 0; i < sysInfo.dwNumberOfProcessors - 8; ++i) {
        CreateThread(NULL, 0, WorkerThread, hIOCP, 0, NULL);
    }
    {
        lock_guard lock(coutMutex);
        std::cout << __func__ << ":Server is listening on port 9000" << std::endl;
    }

    while (true) {
        SOCKET client = accept(listenSock, NULL, NULL);
        CreateIoCompletionPort((HANDLE)client, hIOCP, (ULONG_PTR)client, 0);

        IOContext* ctx = new IOContext();
        ZeroMemory(&ctx->overlapped, sizeof(OVERLAPPED));
        postReadHeader(client, ctx);
    }

    WSACleanup();
    return 0;
}