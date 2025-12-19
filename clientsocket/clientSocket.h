#pragma once

#include <iostream>
#include <string>
#include <atomic>
#include <thread>
#include <memory>
#include <mutex>
//#include "../utils/threadSafeQueue.h"
#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
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
    // Incoming message queue and reader thread to avoid blocking reads
    //threadSafeQueue<string> m_IncomingQueue;
    jthread m_ReaderThread;
    Logger& m_Logger;    
public:
    // Constructor
    ClientSocket(Logger& logger, const string& ip, const char* m_portHostName);
    
    //connect - establish connection to server
    int connect();
    
    //SocketClosed - close the socket connection
    void socketClosed();
    
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
    int sendMessage(const string& message);

    // sendMessage - send message size to server
    // messageSize - size of the message to send
    int sendMessage(int messageSize);

    // readMessage - read a single message from server
    // message - is output parameter to hold the received message
    // returns number of bytes read, or -1 on error/disconnection
    int readMessage(string &message);

    // readSize - read the size of the next message from server
    // returns size of the next message, or -1 on error/disconnection   
    int readSize();

    // logErrorMessage - prints error code and description
    // errorCode: the error code to be printed
    void logErrorMessage(int errorCode);

    // getLoggerFileOpen - verify if the log file it's open
    bool getLogggerFileOpen();
};

// LoggerFactory - Singleton factory for ClientSocket instance
struct ClientSocketFactory{
    static ClientSocket& getInstance(const string& ip, const char* portHostName, const string& logFileName){
        static Logger& logger = LoggerFactory::getInstance(logFileName);
        static ClientSocket instance(logger, ip, portHostName);
        return instance;
    }
};
