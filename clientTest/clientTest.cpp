#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <cstdint>

#pragma comment(lib, "Ws2_32.lib")
using namespace std;

int processMessage(const std::string& msg, SOCKET sd){
    // Prepare a length-prefixed message
    
    uint32_t len = (uint32_t)msg.size();

    // Send header (length)
    send(sd, reinterpret_cast<const char*>(&len), sizeof(len), 0);
    // Send body
    send(sd, msg.data(), (int)msg.size(), 0);

    // Receive echoed header
    uint32_t echoLen;
    char sizeMgs[4 + 1]{0};
    int ret = recv(sd, sizeMgs, sizeof(sizeMgs) - 1, MSG_WAITALL);
    if (ret <= 0) {
        std::cerr << "recv header failed\n";
        closesocket(sd);
        WSACleanup();
        return -1;
    }
    echoLen = atoi(sizeMgs);
    //cout << "Echoed length: " << sizeMgs << "\n";
    //cout << "Echoed length (uint32_t): " << echoLen << "\n";

    // Receive echoed body
    std::string echoMsg(echoLen + 1, 0);
    ret = recv(sd, echoMsg.data(), echoLen, MSG_WAITALL);
    if (ret <= 0) {
        std::cerr << "recv body failed\n";
    } else {
        std::cout << "Echoed message: " << echoMsg << "\n";
    }
    return 0;
}

int main(int argc, char* argv[]) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "socket() failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9000);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "connect() failed: " << WSAGetLastError() << "\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }
    int nr = 1;
    if (argc > 1){
        nr = atoi(argv[1]); 
    }
    string msg = "Hello from client!";
    if (argc == 3){
        msg = string(argv[2]);   
    }
    for (int i = 0; i < nr; i++)    
    {
        processMessage(msg, sock);
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}