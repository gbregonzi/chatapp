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
#include "../utils/logger.h"

using namespace std;

class ClientSocket {
private:   
    string m_ip;
    const char* m_PortHostName;
    int m_sockfd;    
    atomic<bool> m_chatActive{false}; // Flag to control chat activity
    Logger& m_Logger;    
public:
    // Constructor
    ClientSocket(Logger& logger, const string& ip, const char* m_portHostName);
    
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

// LoggerFactory - Singleton factory for ClientSocket instance
struct ClientSocketFactory{
    static ClientSocket& getInstance(const string& ip, const char* portHostName){
        static Logger& logger = LoggerFactory::getInstance("clientSocket.log");
        static ClientSocket instance(logger, ip, portHostName);
        return instance;
    }
};
