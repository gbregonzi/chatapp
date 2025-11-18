#include <WinSock2.h>
#include <winsock.h>
#include <ws2def.h>
#include <vector>
#include <iostream>


class ClientContext {
public:
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

class IOCPServer {
private:
    HANDLE iocp;
    std::vector<HANDLE> workerThreads;
    static const int THREAD_COUNT = 4; // Example thread count

    static DWORD WINAPI WorkerThread(LPVOID lpParam) {
        HANDLE iocp = (HANDLE)lpParam;
        DWORD bytesTransferred;
        ULONG_PTR completionKey;
        LPOVERLAPPED lpOverlapped;

        while (true) {
            BOOL result = GetQueuedCompletionStatus(iocp, &bytesTransferred, &completionKey, &lpOverlapped, INFINITE);
            if (!result || lpOverlapped == nullptr) continue;

            ClientContext* context = reinterpret_cast<ClientContext*>(completionKey);

            if (bytesTransferred == 0) {
                closesocket(context->socket);
                delete context;
                continue;
            }

            send(context->socket, context->buffer, bytesTransferred, 0);

            ZeroMemory(&context->overlapped, sizeof(OVERLAPPED));
            DWORD flags = 0;
            WSARecv(context->socket, &context->wsabuf, 1, NULL, &flags, &context->overlapped, NULL);
        }
        return 0;
    }

public:
    IOCPServer() {
        iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
        for (int i = 0; i < THREAD_COUNT; ++i) {
            HANDLE thread = CreateThread(NULL, 0, WorkerThread, iocp, 0, NULL);
            workerThreads.push_back(thread);
        }
    }

    void associateSocket(SOCKET clientSocket) {
        ClientContext* context = new ClientContext(clientSocket);
        CreateIoCompletionPort((HANDLE)clientSocket, iocp, (ULONG_PTR)context, 0);

        DWORD flags = 0;
        WSARecv(clientSocket, &context->wsabuf, 1, NULL, &flags, &context->overlapped, NULL);
    }

    ~IOCPServer() {
        // Cleanup code for threads and IOCP handle would go here
    }
};

int main() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return 1;
    }

    IOCPServer server;

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create listen socket." << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = INADDR_ANY;
    service.sin_port = htons(8080); // Listening on port 8080

    if (bind(listenSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
        std::cerr << "Bind failed." << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed." << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server listening on port 8080..." << std::endl;

    while (true) {
        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Accept failed." << std::endl;
            continue;
        }

        server.associateSocket(clientSocket);
    }

    closesocket(listenSocket);
    WSACleanup();
    return 0;
}
