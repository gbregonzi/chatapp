
#include <iostream>
#include <string>
#include <unistd.h>
#include <cstring>
#include <thread>
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
//    #pragma comment(lib, "ws2_32.lib")
#else
    #include <arpa/inet.h>
#endif

#include "clientSocket.h"

ClientSocket::ClientSocket(const string& ip, int port) : m_ip(ip), m_port(port) {
        cout << "Class ClientSocket created for " << ip << ":" << port << endl;
}

int ClientSocket::connect()
{
    cout << "Connecting to " << m_ip << ":" << m_port << "..." << endl;
#ifdef _WIN32
    WSADATA d;
    if (WSAStartup(MAKEWORD(2,2), &d)) {
        cerr << "Failed to initialize Winsock!" << endl;
        exit(EXIT_FAILURE);
    }
#endif
    m_sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(m_port);
    inet_pton(AF_INET, m_ip.c_str(), &server_addr.sin_addr);
    if (::connect(m_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        cerr << "Connection failed!" << endl;
        return -1;
    }
    m_chatActive.store(true); // Mark chat as active
    return 0;
}

void ClientSocket::readMessageThread()
{
    char buffer[1024];
    while (m_chatActive.load())
    {
        cout << "Waiting for message from server..." << endl;
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_received = recv(m_sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0)
        {
            buffer[bytes_received] = '\0'; // Null-terminate the received data
            cout << "Received from server: " << buffer << endl;
        }
        else if (bytes_received == 0)
        {
            cout << "Server closed the connection." << endl;
            break;
        }
        else if (strcmp(buffer, "exit") == 0)
        {
            cout << "Exit command received. Closing connection." << endl;
            break;
        }
        else
        {
            cerr << "Error receiving data from server." << endl;
        }
    }
    disconnect();
    m_chatActive.store(false); // Ensure chat is marked as inactive   
}
void ClientSocket::startReadMessageThead()
{
    thread readThread(&ClientSocket::readMessageThread, this);
    readThread.detach(); // Detach the thread to run independently
}

void ClientSocket::startSendMessageThread()
{
    thread sendThread(&ClientSocket::sendMessageThread, this);
    sendThread.detach(); // Detach the thread to run independently
}

void ClientSocket::sendMessageThread()
{
    while (m_chatActive.load())
    {
        string message;
        cout << "Enter message to send (type 'exit' to quit): ";
        getline(cin, message);
        sendMessage(message); 
        if (message == "exit")
        {
            break;
        }
    }
    m_chatActive.store(false);
    disconnect();
}
void ClientSocket::sendMessage(const string& message)
{
    ssize_t bytes_sent = send(m_sockfd, message.c_str(), message.size(), 0);
    if (bytes_sent < 0)
    {
        cerr << "Error sending data to server." << endl;
    }
    else
    {
        cout << "Sent to server: " << message << endl;
    }
}

void ClientSocket::disconnect()
{
    cout << "Disconnecting from " << m_ip << ":" << m_port << "..." << endl;
    close(m_sockfd);
}
ClientSocket::~ClientSocket()
{
    cout << "ClientSocket for " << m_ip << ":" << m_port << " destroyed." << endl;
    disconnect();
}
