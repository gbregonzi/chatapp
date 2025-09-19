#pragma once

#include <iostream>
#include <string>
#include <unistd.h>
#include <atomic>
#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <netinet/in.h>
#endif

using namespace std;

class ClientSocket {
private:   
    string m_ip;
    int m_port;
    int m_sockfd;    
    atomic<bool> m_chatActive{false}; // Flag to control chat activity    
public:
    // Constructor
    ClientSocket(const string& ip, int port);
    
    //connect - establish connection to server
    int connect();
    
    //SocketClosed - close the socket connection
    void SocketClosed();
    
    //Destructor
    ~ClientSocket();
    
    //Delete copy constructor and assignment operator to prevent copying
    ClientSocket(const ClientSocket&) = delete;
    ClientSocket& operator=(const ClientSocket&) = delete;  
    
    //Delete move constructor and assignment operator to prevent moving 
    ClientSocket(ClientSocket&&) = delete;
    ClientSocket& operator=(ClientSocket&&) = delete;  
    
    // sendMessage - send a single message to server
    // message - the message to send
    // returns number of bytes sent, or -1 on error
    size_t sendMessage(const string& message);

    // readMessage - read a single message from server
    // message - is output parameter to hold the received message
    // returns number of bytes read, or -1 on error/disconnection
    size_t readMessage(string &message);

    // LogErrorMessage - prints error code and description
    // errorCode: the error code to be printed
    void LogErrorMessage(int errorCode);

};