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

constexpr int SUCCESS = 0;
constexpr int FAILURE = -1;

using namespace std;

ServerSocket::ServerSocket()
{
#ifdef _WIN32
    WSADATA d;
    if (WSAStartup(MAKEWORD(2,2), &d)) {
        cerr << __func__ << ":" << "Failed to initialize Winsock!" << endl;
        exit(EXIT_FAILURE);
    }
#endif

    m_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_sockfd < 0)
    {
        cerr << "Socket creation failed!" << std::endl;
        logErrorMessage(errno);
        exit(EXIT_FAILURE);
    }

    cout << __func__ << ":" << "Server socket created successfully." << std::endl;

    memset(&m_ServerAddr, 0, sizeof(m_ServerAddr));

    m_ServerAddr.sin_family = AF_INET;
    m_ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    m_ServerAddr.sin_port = htons(PORT);

    char optval{1};
    int optlen{sizeof(optval)};
    if (setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, optlen) < 0)
    {
        cerr << __func__ << ":" << "Setsockopt failed!" << endl;
        logErrorMessage(errno);
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
            cerr << __func__ << ":" << "Bind failed! Retrying..." << attempts << "/" << MAX_PORT_TRIES << endl;
            cout << __func__ << ":" << "Trying port: " << randomPort << endl;
            logErrorMessage(errno);
            continue;
        }
        attempts++;
        break;
    }
    cout << __func__ << ":" << "Number of attempts to bind: " << attempts << endl;
    if (attempts == MAX_PORT_TRIES)
    {
        cerr << __func__ << ":" << "Failed to bind after multiple attempts!" << endl;
        exit(EXIT_FAILURE);
    }

    cout << __func__ << ":" << "Server bound to port " << randomPort << "." << endl;

    if (listen(m_sockfd, MAX_QUEUE_CONNECTINON) < 0)
    {
        cerr << __func__ << ":" << "Listen failed!" << endl;
        logErrorMessage(errno);
        exit(EXIT_FAILURE);
    }
    getHostNameIP();
    m_IsConnected.store(true);
    threadBradcastMessage();
    //serverSendBroadcastMessage();
}

void ServerSocket::logErrorMessage(int errorCode)
{
    cout << __func__ << ":" << "Error code: " << errorCode << endl;
    cout << __func__ << ":" << "Error description: " << strerror(errorCode) << endl
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

int ServerSocket::getServerSocket() const { 
    return m_sockfd; 
}    

void ServerSocket::getHostNameIP()
{
    char hostname[BUFFER_SIZE];
    if (gethostname(hostname, sizeof(hostname)) == 0)
    {
        cout << __func__ << ":" << "Server name: " << hostname << endl;
    }
    else
    {
        cerr << __func__ << ":" << "Gethostname failed!" << endl;
        logErrorMessage(errno);
    }

    hostent *hostIP = gethostbyname(hostname);
    if (hostIP == nullptr)
    {
        cerr << __func__ << ":" << "Gethostbyname failed!" << endl;
        logErrorMessage(errno);
        exit(EXIT_FAILURE);
    }
    char ip[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, hostIP->h_addr, ip, sizeof(ip)) == nullptr)
    {
        cerr << __func__ << ":" << "Inet_ntop failed!" << endl;
        logErrorMessage(errno);
        exit(EXIT_FAILURE);
    }
    cout << __func__ << ":" << "Host IP: " << ip << endl;
}

bool ServerSocket::getClientIP(int sd)
{
    char ip4[INET_ADDRSTRLEN];
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    if (getpeername(sd, (struct sockaddr *)&clientAddr, &clientAddrLen) == 0)
    {
        inet_ntop(AF_INET, &(clientAddr.sin_addr), ip4, INET_ADDRSTRLEN);
        cout << __func__ << ":" << " Client IP: " << ip4 << " Port: " << ntohs(clientAddr.sin_port) << endl;
        return true;
    }
    cerr << __func__ << ":" << " Getpeername failed! Error: " << errno << endl;
    return false;
}

int ServerSocket::listenClientConnections()
{
    cout << __func__ << ":" << "\n**********************************************" << endl;
    cout << __func__ << ":" << " Server is ready to accept connections." << endl;
    socklen_t clientAddrLen = sizeof(sockaddr);
    int  clientSocket = accept(m_sockfd, (struct sockaddr *)&m_ServerAddr, &clientAddrLen);
    if (getIsConnected() == false)
    {
        cerr << __func__ << ":" << " Server is shuting down. Cannot accept new connections." << endl; 
        return SUCCESS;
    }
    if (clientSocket < 0)
    {
        cerr << __func__ << ":" << " Accept failed!" << endl;
        logErrorMessage(errno);
        return FAILURE;
    }
    if (!getClientIP(clientSocket))
    {
        return FAILURE;
    }        
    
    cout << __func__ << ":" << " Client connected successfully." << endl;
    auto result = clientSockets.emplace(clientSocket);
    if (!result.second)
    {
        cerr << __func__ << ":" << " Failed to add client socket to the set." << endl;
        CLOSESOCKET(clientSocket);
        return FAILURE;
    }   
    return clientSocket;
}

ServerSocket::~ServerSocket()
{
    CLOSESOCKET(m_sockfd);
#ifdef _WIN32
    WSACleanup();
#endif
    cout << __func__ << ":" << " ServerSocket class destroyed." << endl;
}

bool ServerSocket::writeToClient(int sd, const string_view message)
{
    size_t bytesSent = send(sd, message.data(), message.length(), 0);
    if (bytesSent < 0)
    {
        cerr << __func__ << ":" << " Send to client failed!" << endl;
        logErrorMessage(errno);
        return false;
    }
    return true;
}

size_t ServerSocket::readMessage(string &message, int sd){
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    size_t bytes_received = recv(sd, buffer, sizeof(buffer) - 1, 0);
    if (getIsConnected() == false)
    {
        cerr << __func__ << ":" << "Server is shuting down. Cannot read messages." << endl; 
        return SUCCESS;
    }
    if (strcmp(buffer, "quit") == 0)
    {
        cout << __func__ << ":" << "Quit command received. Closing connection." << endl;
        return SUCCESS;
    }
    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0'; // Null-terminate the received data
        getClientIP(sd);
        cout << __func__ << ":" << "Message size: " << bytes_received << endl;
        cout << __func__ << ":" << "Message from client: " << buffer << endl;
        message = string(buffer);
        return bytes_received;
    }
    if (bytes_received == 0)
    {
        //cout << "Client closed the connection." << endl;
        return SUCCESS;
    }
    else
    {
        cerr << __func__ << ":" << "Error receiving data from client." << endl;
        logErrorMessage(errno);
    }
    return FAILURE; // Indicate error or connection closed
}

size_t ServerSocket::getClientCount(){
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
//             logErrorMessage(errno);
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
                cout << __func__ << ":" << "sd: " << sd << endl;
                getClientIP(sd);
                cout << __func__ << ":" << "Send message to client: " << message << endl;
            }
            else
            {
                this_thread::sleep_for(chrono::milliseconds(100));
            }
        }
    });
    readThread.detach();
}

void ServerSocket::SocketClosed(int sd)
{
    CLOSESOCKET(sd);
    cout << __func__ << ":" << " Socket closed." << endl;
}

unordered_set<int> ServerSocket::getClientSockets() const {
    return clientSockets;
}

int ServerSocket::addBroadcastTextMessage(string message) {
    if (m_IsConnected.load())
    {
        for (const auto &sd : clientSockets)
        {
            broacastMessageQueue.push(make_pair(sd, message));
        }
        return SUCCESS; // Indicate success
    }
    return FAILURE; // Indicate failure if not connected
}
