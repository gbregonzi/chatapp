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
constexpr int MAX_PORT_TRIES{10};

using namespace std;

ServerSocket::ServerSocket(unique_ptr<OutputStream>& outputStream): m_cout(outputStream)
{
#ifdef _WIN32
    WSADATA d;
    if (WSAStartup(MAKEWORD(2,2), &d)) {
        *m_cout << __func__ << ":" << "Failed to initialize Winsock!\n";
        exit(EXIT_FAILURE);
    }
#endif
    m_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_sockfd < 0)
    {
        m_cout->log("Socket creation failed!");
        logErrorMessage(errno);
        exit(EXIT_FAILURE);
    }
    *m_cout << "m_cout:" << m_cout << "\n";

    *m_cout << __func__ << ":" << "Server created successfully.\n";

    memset(&m_ServerAddr, 0, sizeof(m_ServerAddr));

    m_ServerAddr.sin_family = AF_INET;
    m_ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    m_ServerAddr.sin_port = htons(PORT);

    char optval{1};
    int optlen{sizeof(optval)};
    if (setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, optlen) < 0)
    {
        *m_cout << __func__ << ":" << "Setsockopt failed!\n";
        logErrorMessage(errno);
        exit(EXIT_FAILURE);
    }
    auto randomPort = PORT;
    auto attempts{0};
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
            *m_cout << __func__ << ":" << "Bind failed! Retrying..." << attempts << "/" << MAX_PORT_TRIES << "\n";
            *m_cout << __func__ << ":" << "Trying port: " << randomPort << "\n";
            logErrorMessage(errno);
            continue;
        }
        attempts++;
        break;
    }
    *m_cout << __func__ << ":" << "Number of attempts to bind: " << attempts << "\n";
    if (attempts == MAX_PORT_TRIES)
    {
        *m_cout << __func__ << ":" << "Failed to bind after multiple attempts!\n";
        exit(EXIT_FAILURE);
    }

    *m_cout << __func__ << ":" << "Server bound to port " << randomPort << ".\n";

    if (listen(m_sockfd, MAX_QUEUE_CONNECTINON) < 0)
    {
        *m_cout << __func__ << ":" << "Listen failed!\n";
        logErrorMessage(errno);
        exit(EXIT_FAILURE);
    }
    getHostNameIP();
    m_sToken = m_Source.get_token();
    m_IsConnected.store(true);
    threadBroadcastMessage();
}

void ServerSocket::logErrorMessage(int errorCode)
{
    *m_cout << __func__ << ":" << "Error code: " << errorCode << "\n";
    *m_cout << __func__ << ":" << "Error description: " << strerror(errorCode) << "\n\n";
}

mutex& ServerSocket::getMutex() {
    return m_mutex;
}

bool ServerSocket::getIsConnected() const
{
    return m_IsConnected.load();
}

void ServerSocket::setIsConnected(bool isConnected) 
{
    m_IsConnected.store(isConnected);
}

void ServerSocket::closeSocketServer() 
{
    CLOSESOCKET(m_sockfd);
    m_BroadcastThread.request_stop();
    *m_cout << __func__ << ":" << " Server socket closed.\n";    
}
void ServerSocket::closeAllClientSockets() 
{
    for (const auto &sd : m_ClientSockets){
        CLOSESOCKET(sd);
    }
    m_ClientSockets.clear();
    *m_cout << __func__ << ":" << " All client sockets closed.\n";    
}

void ServerSocket::getHostNameIP()
{
    char hostname[BUFFER_SIZE];
    if (gethostname(hostname, sizeof(hostname)) == 0)
    {
        *m_cout << __func__ << ":" << "Server name: " << hostname << "\n";
    }
    else
    {
        *m_cout << __func__ << ":" << "Gethostname failed!\n";
        logErrorMessage(errno);
    }

    hostent *hostIP = gethostbyname(hostname);
    if (hostIP == nullptr)
    {
        *m_cout << __func__ << ":" << "Gethostbyname failed!\n";
        logErrorMessage(errno);
        exit(EXIT_FAILURE);
    }
    char ip[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, hostIP->h_addr, ip, sizeof(ip)) == nullptr)
    {
        *m_cout << __func__ << ":" << "Inet_ntop failed!\n";
        logErrorMessage(errno);
        exit(EXIT_FAILURE);
    }
    *m_cout << __func__ << ":" << "Host IP: " << ip << "\n";
}

bool ServerSocket::getClientIP(int sd)
{
    char ip4[INET_ADDRSTRLEN];
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    if (getpeername(sd, (struct sockaddr *)&clientAddr, &clientAddrLen) == 0)
    {
        inet_ntop(AF_INET, &(clientAddr.sin_addr), ip4, INET_ADDRSTRLEN);
        *m_cout << __func__ << ":" << " Client IP: " << ip4 << " Port: " << ntohs(clientAddr.sin_port) << "\n";
        return true;
    }
    *m_cout << __func__ << ":" << " Getpeername failed! Error: " << errno << "\n";
    return false;
}

int ServerSocket::handleConnections()
{
    *m_cout << __func__ << ":" << "Server is ready to accept connections.\n";
    socklen_t clientAddrLen = sizeof(sockaddr);
    int  clientSocket = accept(m_sockfd, (struct sockaddr *)&m_ServerAddr, &clientAddrLen);
    if (getIsConnected() == false)
    {
        *m_cout << __func__ << ":" << "Server is shutting down. Cannot accept new connections.\n"; 
        return SUCCESS;
    }
    if (clientSocket < 0)
    {
        *m_cout << __func__ << ":" << "Accept failed!\n";
        logErrorMessage(errno);
        return FAILURE;
    }
    if (!getClientIP(clientSocket))
    {
        return FAILURE;
    }        
    
    *m_cout << __func__ << ":" << " Client connected successfully.\n";
    auto result = m_ClientSockets.emplace(clientSocket);
    if (!result.second)
    {
        *m_cout << __func__ << ":" << " Failed to add client socket to the set.\n";
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
    *m_cout << __func__ << ":" << " ServerSocket class destroyed.\n";
}

bool ServerSocket::sendMessage(int sd, const string_view message)
{
    size_t bytesSent = send(sd, message.data(), message.length(), 0);
    if (bytesSent < 0)
    {
        *m_cout << __func__ << ":" << " Send to client failed!\n";
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
        *m_cout << __func__ << ":" << "Server is shuting down. Cannot read messages.\n"; 
        return SUCCESS;
    }
    if (strcmp(buffer, "quit") == 0)
    {
        *m_cout << __func__ << ":" << "Quit command received. Closing connection.\n";
        return SUCCESS;
    }
    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0'; // Null-terminate the received data
        getClientIP(sd);
        *m_cout << __func__ << ":" << "Message size: " << bytes_received << "\n";
        *m_cout << __func__ << ":" << "Message from client: " << buffer << "\n";
        message = string(buffer);
        return bytes_received;
    }
    if (bytes_received == 0)
    {
        // Client closed the connection
        return SUCCESS;
    }
    else
    {
        *m_cout << __func__ << ":" << "Error receiving data from client.\n";
        logErrorMessage(errno);
    }
    return FAILURE; // Indicate error or connection closed
}

size_t ServerSocket::getClientCount(){
    return m_ClientSockets.size();
}

void ServerSocket::threadBroadcastMessage() {
    *m_cout << __func__ << ":" << "Broadcast thread started.\n";

    m_BroadcastThread = jthread([this](stop_token token) {
        while (!token.stop_requested()) {
            pair<int, string> front;
            {
                lock_guard<mutex> lock(m_mutex);
                if (!m_BroadcastMessageQueue.empty()) {
                
                    front = m_BroadcastMessageQueue.front();
                    m_BroadcastMessageQueue.pop();
                }
            }
            
            if (front.first > 0) {
                int sd = front.first;
                const string& message = front.second;
                if (sendMessage(sd, message)) {
                    getClientIP(sd);
                    *m_cout << "threadBroadcastMessage:sd: " << sd << ": Sent message to client: " << message << "\n";
                } else {
                    *m_cout << "threadBroadcastMessage:Failed to send message to client: " << sd << "\n";
                }
                m_cout->log("threadBroadcastMessage:***********************************************");
            }
            this_thread::yield;
        }
        m_cout->log("threadBroadcastMessage:Broadcast thread stoped");
    });
}

void ServerSocket::closeSocket(int sd)
{
    CLOSESOCKET(sd);
    m_ClientSockets.erase(sd);
    *m_cout << __func__ << ":" << " Socket closed.\n";
}

unordered_set<int> ServerSocket::getClientSockets() const {
    return m_ClientSockets;
}

int ServerSocket::addBroadcastTextMessage(const string message, int sd) {
    if (m_IsConnected.load())
    {
        m_BroadcastMessageQueue.push(make_pair(sd, message));
        return SUCCESS;
    }
    return FAILURE; // Indicate failure if not connected
}

int ServerSocket::addBroadcastTextMessage(const string message) {
    for (const auto &sd : m_ClientSockets){
        if (addBroadcastTextMessage(message, sd) == FAILURE){
            return FAILURE;
        }
    }
    m_condVar.notify_one();
    return SUCCESS;
}