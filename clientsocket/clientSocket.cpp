#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <unistd.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
#else
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <sys/socket.h>
#endif

#include "clientSocket.h"


ClientSocket::ClientSocket(Logger& logger, const string& ip, const char* portHostName) : 
        m_Logger(logger), m_ip(ip), m_PortHostName(portHostName) {
        m_Logger.log(LogLevel::Info, "{}:Class ClientSocket created for {}:{}",__func__, ip, m_PortHostName);
}

int ClientSocket::connect()
{
    m_Logger.log(LogLevel::Info, "{}:Connecting...", __func__);
#ifdef _WIN32
    WSADATA d;
    if (WSAStartup(MAKEWORD(2,2), &d)) {
        m_Logger.log(LogLevel::Error, "{}:Failed to initialize Winsock!", __func__);
        exit(EXIT_FAILURE);
    }
#endif
    if (m_ip.find('.') == string::npos) {
        struct addrinfo hints{}, *res = nullptr;
        int status = getaddrinfo(m_ip.c_str(), m_PortHostName, &hints, &res);
        if (status != 0) {
            m_Logger.log(LogLevel::Error, "{}:getaddrinfo error:{} ",__func__, gai_strerror(status));
            return EXIT_FAILURE;
        }
        m_sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (m_sockfd < 0) {
            m_Logger.log(LogLevel::Error, "{}:Socket creation failed! Erro:{}",__func__, strerror(errno));
            freeaddrinfo(res);
            return EXIT_FAILURE;
        }
        if (::connect(m_sockfd, res->ai_addr, res->ai_addrlen) < 0) {
            m_Logger.log(LogLevel::Error, "{}:Connection failed! Erro:{}",__func__, strerror(errno));
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
            m_Logger.log(LogLevel::Error,"{}:Connection failed! Erro:{}",__func__, strerror(errno));
            return -1;
        }
    }
    m_Logger.log(LogLevel::Info, "{}:Connected", __func__);
    m_chatActive.store(true); // Mark chat as active
    return 0;
}

size_t ClientSocket::readMessage(string &message){
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    size_t bytes_received = recv(m_sockfd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received == ULLONG_MAX || bytes_received == 0)
    {
        m_Logger.log(LogLevel::Info, "{}:Server closed the connection.",__func__);
    }
    else if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0'; // Null-terminate the received data
        m_Logger.log(LogLevel::Debug, "{}:Message size:{}",__func__, bytes_received);
        m_Logger.log(LogLevel::Debug, "{}:Received from server:{}",__func__, buffer);
        message = string(buffer);
        return bytes_received;
    }
    else if (strcmp(buffer, "quit") == 0)
    {
        m_Logger.log(LogLevel::Debug, "{}:Quit command received, closing connection.",__func__);
    }
    else
    {
        m_Logger.log(LogLevel::Error, "{}:Error receiving data from server.",__func__);
    }
    return -1; // Indicate error or connection closed
}

size_t ClientSocket::sendMessage(const string& message)
{
    size_t bytes_sent = send(m_sockfd, message.c_str(), message.size(), 0);
    if (bytes_sent < 0)
    {
        m_Logger.log(LogLevel::Error,"{}:Error sending data to server.",__func__);
        return -1;
    }
    m_Logger.log(LogLevel::Debug, "{}:Sent to server:{}",__func__, message);
    return bytes_sent;
}

void ClientSocket::SocketClosed()
{
    m_Logger.log(LogLevel::Info, "{}:Disconnecting from:{}:{}...",__func__, m_ip, m_PortHostName);
    close(m_sockfd);
}
ClientSocket::~ClientSocket()
{
#ifdef _WIN32
    WSACleanup();
#endif
    m_Logger.log(LogLevel::Info,"{}:ClientSocket for:{}:{} destroyed.",__func__, m_ip, m_PortHostName);
}

void ClientSocket::logErrorMessage(int errorCode)
{
    m_Logger.log(LogLevel::Error, "{}:Error code:{}",__func__, errorCode);
    m_Logger.log(LogLevel::Error, "{}:Error description:{}",__func__, strerror(errorCode));
}

bool ClientSocket::getLogggerFileOpen(){
    return m_Logger.isOpen();
}