#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    typedef int socklen_t;
    #define CLOSESOCKET closesocket
    //#define GETSOCKETERRNO() (WSAGetLastError())
#else
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #define CLOSESOCKET close
    //#define GETSOCKETERRNO() (errno)
#endif
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>

#include "chatserver.h"

constexpr int SUSCESS = 0;
constexpr int FAILURE = -1;

using namespace std;

ServerSocket::ServerSocket()
{
#ifdef _WIN32
    WSADATA d;
    if (WSAStartup(MAKEWORD(2,2), &d)) {
        cerr << "Failed to initialize Winsock!" << endl;
        exit(EXIT_FAILURE);
    }
#endif

    m_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_sockfd < 0)
    {
        cerr << "Socket creation failed!" << std::endl;
        getError(errno);
        exit(EXIT_FAILURE);
    }

    cout << "Server socket created successfully." << std::endl;

    memset(&m_ServerAddr, 0, sizeof(m_ServerAddr));

    m_ServerAddr.sin_family = AF_INET;
    m_ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    m_ServerAddr.sin_port = htons(PORT);

    char optval{1};
    int optlen{sizeof(optval)};
    if (setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, optlen) < 0)
    {
        cerr << "Setsockopt failed!" << endl;
        getError(errno);
        exit(EXIT_FAILURE);
    }
    auto randomPort = PORT;
    auto attempts{0};
    constexpr int MAX_PORT_TRIES{10};
    vector<int> triedPorts;
    while (attempts < MAX_PORT_TRIES)
    {

        if (bind(m_sockfd, (struct sockaddr *)&m_ServerAddr, sizeof(m_ServerAddr)) < 0)
        {
            while (true)
            {
                this_thread::sleep_for(chrono::milliseconds(500));
                randomPort = PORT + (rand() % 10); // Random port between 8080 and 8090
                m_ServerAddr.sin_port = htons(randomPort);
                if (find(triedPorts.begin(), triedPorts.end(), randomPort) == triedPorts.end())
                {
                    triedPorts.push_back(randomPort);
                    attempts++;
                    break;
                }
            }
            cerr << "Bind failed! Retrying..." << attempts << "/" << MAX_PORT_TRIES << endl;
            cout << "Trying port: " << randomPort << endl;
            getError(errno);
            continue;
        }
        attempts++;
        break;
    }
    cout << "Number of attempts to bind: " << attempts << endl;
    if (attempts == MAX_PORT_TRIES)
    {
        cerr << "Failed to bind after multiple attempts!" << endl;
        exit(EXIT_FAILURE);
    }

    cout << "Server bound to port " << randomPort << "." << endl;

    if (listen(m_sockfd, MAX_QUEUE_CONNECTINON) < 0)
    {
        cerr << "Listen failed!" << endl;
        getError(errno);
        exit(EXIT_FAILURE);
    }
    getHostNameIP();
    m_IsConnected.store(true);
    threadBradcastMessage();
    //serverSendBroadcastMessage();
}

void ServerSocket::getError(int errorCode)
{
    cout << "Error code: " << errorCode << endl;
    cout << "Error description: " << strerror(errorCode) << endl
         << endl;
}

bool ServerSocket::getIsConnected() const
{
    return m_IsConnected.load();
}
void ServerSocket::setIsConnected(bool isConnected) 
{
    m_IsConnected.store(isConnected);
}

void ServerSocket::getHostNameIP()
{
    char hostname[BUFFER_SIZE];
    if (gethostname(hostname, sizeof(hostname)) == 0)
    {
        cout << "Server name: " << hostname << endl;
    }
    else
    {
        cerr << "Gethostname failed!" << endl;
        getError(errno);
    }

    hostent *hostIP = gethostbyname(hostname);
    if (hostIP == nullptr)
    {
        cerr << "Gethostbyname failed!" << endl;
        getError(errno);
        exit(EXIT_FAILURE);
    }
    char ip[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, hostIP->h_addr, ip, sizeof(ip)) == nullptr)
    {
        cerr << "Inet_ntop failed!" << endl;
        getError(errno);
        exit(EXIT_FAILURE);
    }
    cout << "Host IP: " << ip << endl;
}

bool ServerSocket::getClientIP(int sd)
{
    char ip4[INET_ADDRSTRLEN];
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    if (getpeername(sd, (struct sockaddr *)&clientAddr, &clientAddrLen) == 0)
    {
        inet_ntop(AF_INET, &(clientAddr.sin_addr), ip4, INET_ADDRSTRLEN);
        cout << "\n**********************************************" << endl;
        cout << "Client IP: " << ip4 << " Port: " << ntohs(clientAddr.sin_port) << endl;
        return true;
    }
    cerr << "Getpeername failed! Error: " << errno << endl;
    return false;
}

void ServerSocket::listenClientConnections()
{
    while (true)
    {
        cout << "\n**********************************************" << endl;
        cout << "Server is ready to accept connections." << endl;
        socklen_t clientAddrLen = sizeof(sockaddr);
        auto clientSocket = accept(m_sockfd, (struct sockaddr *)&m_ServerAddr, &clientAddrLen);
        if (clientSocket < 0)
        {
            cerr << "Accept failed!" << endl;
            getError(errno);
            continue;
        }
        if (!getClientIP(clientSocket))
        {
            closeSocket(clientSocket);
            continue;
        }        
        
        cout << "Client connected successfully." << endl;
        atomic<bool> chatActive{true};
        clientSockets.insert(clientSocket);
        //readFromClientThread(clientSocket, chatActive);
    }
}

ServerSocket::~ServerSocket()
{
    CLOSESOCKET(m_sockfd);
#ifdef _WIN32
    WSACleanup();
#endif
    cout << "Server socket closed." << endl;
}

bool ServerSocket::writeToClient(int sd, const string_view message)
{
    ssize_t bytesSent = send(sd, message.data(), message.length(), 0);
    if (bytesSent < 0)
    {
        cerr << "Send to client failed!" << endl;
        getError(errno);
        return false;
    }
    return true;
    //cout << "Sent to client: " << message << endl;
}

ssize_t ServerSocket::readMessage(string &message){
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    ssize_t bytes_received = recv(m_sockfd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0'; // Null-terminate the received data
        cout << "Received from client: " << buffer << endl;
        message = string(buffer);
        return bytes_received;
    }
    else if (bytes_received == 0)
    {
        cout << "Client closed the connection." << endl;
    }
    else if (strcmp(buffer, "exit") == 0)
    {
        cout << "Exit command received. Closing connection." << endl;
    }
    else
    {
        cerr << "Error receiving data from client." << endl;
    }
    return -1; // Indicate error or connection closed
}

ssize_t ServerSocket::getClientCount(){
    return clientSockets.size();
}

// void ServerSocket::readFromClientThread(int sd, atomic<bool> &chatActive)
// {
//     thread readThread(&ServerSocket::readFromClient, this, sd, ref(chatActive));
//     readThread.detach();
// }

// void ServerSocket::readFromClient(int sd, atomic<bool> &chatActive)
// {
//     char buffer[BUFFER_SIZE];
//     while (chatActive.load())
//     {
//         memset(buffer, 0, sizeof(buffer));
//         ssize_t bytesRead = recv(sd, buffer, sizeof(buffer) - 1, 0);
//         if (bytesRead < 0)
//         {
//             cerr << "Read from client failed!" << endl;
//             getError(errno);
//             break;
//         }
//         else if (bytesRead == 0)
//         {
//             cout << "Client disconnected." << endl;
//             break;
//         }
//         if (strcmp(buffer, "exit") == 0)
//         {
//             cout << "Client requested to end chat." << endl;
//             break;
//         }
//         getClientIP(sd);
//         cout << "Received from client: " << buffer << endl;
//         for (const auto &clientSd : clientSockets)
//         {
//             if (clientSd != sd) // Avoid sending the message back to the sender
//             {
//                 broacastMessageQueue.push(make_pair(clientSd, string(buffer)));
//             }
//         }
//     }
//     chatActive.store(false);
//     CLOSESOCKET(sd);
//     cout << "Chat ended. Client socket closed." << endl;
// }

void ServerSocket::threadBradcastMessage()
{
    thread readThread([this]() {
        while (m_IsConnected.load())
        {
            if (!broacastMessageQueue.empty())
            {
                auto front = broacastMessageQueue.front();
                int sd = front.first;
                string message = front.second;
                if (writeToClient(sd, message)){
                   broacastMessageQueue.pop();     
                } // Echo back to client

                cout << "Send message to client: " << message << endl;
            }
            else
            {
                this_thread::sleep_for(chrono::milliseconds(100));
            }
        }
    });
    readThread.detach();
}

void ServerSocket::closeSocket(int sd)
{
    m_IsConnected.store(false);
    CLOSESOCKET(sd);
    cout << "Server socket closed." << endl;
}

int ServerSocket::addBroadcastTextMessage(string message) {
    if (m_IsConnected.load())
    {
        for (const auto &sd : clientSockets)
        {
            broacastMessageQueue.push(make_pair(sd, message));
        }
        return SUSCESS; // Indicate success
    }
    return FAILURE; // Indicate failure if not connected
}


// void ServerSocket::serverSendBroadcastMessage()
// {
//     thread serverBroadcastThread([this]() {
//         string message;
        
//         while (isConnected.load())
//         {
//             cout << "Enter message to send to client (or 'exit' to quit): ";
//             getline(cin, message);
//             if (message == "exit")
//             {
//                 isConnected.store(false);
//                 break;
//             }
//             for (const auto &sd : clientSockets)
//             {
//                 broacastMessageQueue.push(make_pair(sd, message));
//             }
//         }
//     });
//     serverBroadcastThread.detach();
// }



