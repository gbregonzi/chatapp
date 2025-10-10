
#include <iostream>
#include <string>
#include <unistd.h>
#include <cstring>
#include <thread>
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
#else
    #include <arpa/inet.h>
#endif

#include "clientSocket.h"

ClientSocket::ClientSocket(const string& ip, const char* portHostName) : m_ip(ip), m_PortHostName(portHostName) {
        cout << "Class ClientSocket created for " << ip << ":" << m_PortHostName << endl;
}

int ClientSocket::connect()
{
    cout << "Connecting...\n";
#ifdef _WIN32
    WSADATA d;
    if (WSAStartup(MAKEWORD(2,2), &d)) {
        cerr << "Failed to initialize Winsock!" << endl;
        exit(EXIT_FAILURE);
    }
#endif
    if (m_ip.find('.') == string::npos) {
        struct addrinfo hints{}, *res = nullptr;
        int status = getaddrinfo(m_ip.c_str(), m_PortHostName, &hints, &res);
        if (status != 0) {
            std::cerr << "getaddrinfo error: " << gai_strerror(status) << "\n";
            return EXIT_FAILURE;
        }
        m_sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (m_sockfd < 0) {
            std::cerr << "Socket creation failed! Erro:" << strerror(errno) << "\n";
            freeaddrinfo(res);
            return EXIT_FAILURE;
        }
        if (::connect(m_sockfd, res->ai_addr, res->ai_addrlen) < 0) {
            cerr << "Connection failed! Erro:" << strerror(errno) << "\n";
            freeaddrinfo(res);
            return EXIT_FAILURE;
        }
    }
    else {
        m_sockfd = socket(AF_INET, SOCK_STREAM, 0);

        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(atoi(m_PortHostName));
        inet_pton(AF_INET, m_ip.c_str(), &server_addr.sin_addr);
        if (::connect(m_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            cerr << "Connection failed! Erro:" << strerror(errno) << "\n";
            return -1;
        }
    }
    m_chatActive.store(true); // Mark chat as active
    return 0;
}

size_t ClientSocket::readMessage(string &message){
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    size_t bytes_received = recv(m_sockfd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received == ULLONG_MAX || bytes_received == 0)
    {
        cout << "Server closed the connection." << endl;
    }
    else if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0'; // Null-terminate the received data
        cout << "Message size: " << bytes_received << endl;
        cout << "Received from server: " << buffer << endl << "\n";
        message = string(buffer);
        return bytes_received;
    }
    else if (strcmp(buffer, "quit") == 0)
    {
        cout << "Quit command received, closing connection." << "\n";
    }
    else
    {
        cerr << "Error receiving data from server." << endl;
    }
    return -1; // Indicate error or connection closed
}

size_t ClientSocket::sendMessage(const string& message)
{
    size_t bytes_sent = send(m_sockfd, message.c_str(), message.size(), 0);
    if (bytes_sent < 0)
    {
        cerr << "Error sending data to server." << endl;
        return -1;
    }
    return bytes_sent;
}

void ClientSocket::SocketClosed()
{
    cout << "Disconnecting from " << m_ip << ":" << m_PortHostName << "..." << endl;
    close(m_sockfd);
}
ClientSocket::~ClientSocket()
{
#ifdef _WIN32
    WSACleanup();
#endif
    cout << "ClientSocket for " << m_ip << ":" << m_PortHostName << " destroyed." << endl;
}

void ClientSocket::LogErrorMessage(int errorCode)
{
    cout << "Error code: " << errorCode << endl;
    cout << "Error description: " << strerror(errorCode) << endl
         << endl;
}