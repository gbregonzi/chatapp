#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <netdb.h>
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>

#include "multThreadWebServer.h"

Socket::Socket()
{
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0)
    {
        cerr << "Socket creation failed!" << std::endl;
        exit(EXIT_FAILURE);
    }

    cout << "Server socket created successfully." << std::endl;

    memset(&serverAddr, 0, sizeof(serverAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(PORT);

    int optval{1};
    int optlen{sizeof(optval)};
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &optval, optlen) < 0)
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

        if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
        {
            while (true)
            {
                this_thread::sleep_for(chrono::milliseconds(500));
                randomPort = PORT + (rand() % 10); // Random port between 8080 and 8090
                serverAddr.sin_port = htons(randomPort);
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

    if (listen(serverSocket, MAX_QUEUE_CONNECTINON) < 0)
    {
        cerr << "Listen failed!" << endl;
        getError(errno);
        exit(EXIT_FAILURE);
    }
    getHostNameIP();
    isConnected = true;
    threadBradcastMessage();
    serverSendBroadcastMessage();

}

void Socket::getError(int errorCode)
{
    cout << "Error code: " << errorCode << endl;
    cout << "Error description: " << strerror(errorCode) << endl
         << endl;
}

bool Socket::getIsConnected() const
{
    return isConnected;
}

void Socket::getHostNameIP()
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

bool Socket::getClientIP(int sd)
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

void Socket::listenClientConnections()
{
    while (true)
    {
        cout << "\n**********************************************" << endl;
        cout << "Server is ready to accept connections." << endl;
        socklen_t clientAddrLen = sizeof(sockaddr);
        auto clientSocket = accept(serverSocket, (struct sockaddr *)&serverAddr, &clientAddrLen);
        if (clientSocket < 0)
        {
            cerr << "Accept failed!" << endl;
            getError(errno);
            continue;
        }
        if (!getClientIP(clientSocket))
        {
            close(clientSocket);
            continue;
        }        void serverSendBroadcastMessage();


        cout << "Client connected successfully." << endl;
        atomic<bool> chatActive{true};
        clientSockets.insert(clientSocket);
        readFromClientThread(clientSocket, chatActive);
    }
}

Socket::~Socket()
{
    close(serverSocket);
    cout << "Server socket closed." << endl;
}

bool Socket::writeToClient(int sd, const string_view message)
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

void Socket::readFromClientThread(int sd, atomic<bool> &chatActive)
{
    thread readThread(&Socket::readFromClient, this, sd, ref(chatActive));
    readThread.detach();
}

void Socket::readFromClient(int sd, atomic<bool> &chatActive)
{
    char buffer[BUFFER_SIZE];
    while (chatActive.load())
    {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesRead = read(sd, buffer, sizeof(buffer) - 1);
        if (bytesRead < 0)
        {
            cerr << "Read from client failed!" << endl;
            getError(errno);
            break;
        }
        else if (bytesRead == 0)
        {
            cout << "Client disconnected." << endl;
            break;
        }
        if (strcmp(buffer, "exit") == 0)
        {
            cout << "Client requested to end chat." << endl;
            break;
        }
        getClientIP(sd);
        cout << "Received from client: " << buffer << endl;
        for (const auto &clientSd : clientSockets)
        {
            if (clientSd != sd) // Avoid sending the message back to the sender
            {
                broacastMessageQueue.push(make_pair(clientSd, string(buffer)));
            }
        }
    }
    chatActive.store(false);
    close(sd);
    cout << "Chat ended. Client socket closed." << endl;
}

void Socket::threadBradcastMessage()
{
    thread readThread([this]() {
        while (isConnected.load())
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


void Socket::serverSendBroadcastMessage()
{
    thread serverBroadcastThread([this]() {
        string message;
        
        while (isConnected.load())
        {
            cout << "Enter message to send to client (or 'exit' to quit): ";
            getline(cin, message);
            if (message == "exit")
            {
                isConnected.store(false);
                break;
            }
            for (const auto &sd : clientSockets)
            {
                broacastMessageQueue.push(make_pair(sd, message));
            }
        }
    });
    serverBroadcastThread.detach();
}



