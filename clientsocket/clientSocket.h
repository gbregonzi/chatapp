#include <iostream>
#include <string>
#include <arpa/inet.h>
#include <unistd.h>
#include <atomic>

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
    
    //disconnect - close the socket connection
    void disconnect();
    
    //Destructor
    ~ClientSocket();
    
    //Delete copy constructor and assignment operator to prevent copying
    ClientSocket(const ClientSocket&) = delete;
    ClientSocket& operator=(const ClientSocket&) = delete;  
    
    //Delete move constructor and assignment operator to prevent moving 
    ClientSocket(ClientSocket&&) = delete;
    ClientSocket& operator=(ClientSocket&&) = delete;  
    
    // readMessageThread - thread to read messages from the server
    void readMessageThread();
    
    // startReadMessageThead - start thread to read messages from server
    void startReadMessageThead();
    
    // startSendMessageThread - start thread to send messages to server
    void startSendMessageThread();
    
    // sendMessageThread - send messages to server
    void sendMessageThread();
    
    // sendMessage - send a single message to server
    void sendMessage(const string& message);
};