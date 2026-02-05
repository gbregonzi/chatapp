#ifdef _WIN32
    typedef int socklen_t;
    #define CLOSESOCKET closesocket
#else
    #define CLOSESOCKET close
#endif
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>

#include "chatserver.h"

using namespace std;

ChatServer::ChatServer(Logger& logger, const string& serverName, const string& port): 
                          m_Logger(logger), m_ServerName(serverName), m_PortNumber(port)   
{
    m_Logger.log(LogLevel::Info, "{}:ChatServer class created for {}:{}",__func__, serverName, port);
    m_IsConnected.store(true);
    threadBroadcastMessage();
}
bool ChatServer::createListner(){
    struct addrinfo hints{}, *ai = nullptr, *p = nullptr;
    char hostName[INET6_ADDRSTRLEN];
    char service[20];
    memset (hostName, 0, sizeof(hostName));
    memset (service, 0, sizeof(service));

    memset(&hints, 0, sizeof(hints));
    
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    m_Logger.log(LogLevel::Info, "{}:Staring chatServer", __func__);

    if (getaddrinfo(m_ServerName.c_str(), m_PortNumber.c_str(), &hints, &ai) != 0) {
        m_Logger.log(LogLevel::Error, "{}:Getaddrinfo failed!",__func__);
        m_Logger.log(LogLevel::Error, "{}:{}",__func__, gai_strerror(ERROR_CODE));
        logLastError(m_Logger);
        exit(EXIT_FAILURE);
    }

    for (p = ai; p != nullptr; p = p->ai_next) {
        m_SockfdListener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (m_SockfdListener < 0) {
            continue;
        }
        
        int optlen{sizeof(optval)};
        if (setsockopt(m_SockfdListener, SOL_SOCKET, SO_REUSEADDR, &optval, optlen) < 0)
        {
            m_Logger.log(LogLevel::Error, "{}:Setsockopt failed!",__func__);
            logLastError(m_Logger);
            CLOSESOCKET(m_SockfdListener);
            continue;
        }
        

        if (bind(m_SockfdListener, p->ai_addr, p->ai_addrlen) != -1) {
            break; // Success
        }
        
        m_Logger.log(LogLevel::Error, "{}:Bind failed! Retrying...",__func__);
        logLastError(m_Logger);
        CLOSESOCKET(m_SockfdListener);
    }
    

    if (p == nullptr) {
        m_Logger.log(LogLevel::Error, "{}:Failed to bind socket!",__func__);
        return false;
    }
    m_Logger.log(LogLevel::Info, "{}:Server created and bound successfully.",__func__);
    
    if (getnameinfo(p->ai_addr, p->ai_addrlen, hostName, sizeof(hostName), service, sizeof(service), NI_NOFQDN|NI_NAMEREQD) != 0) {
        m_Logger.log(LogLevel::Error, "{}:getnameinfo fail!",__func__);
        logLastError(m_Logger);
    }
    
    freeaddrinfo(ai); // all done with this
    if (listen(m_SockfdListener, MAX_QUEUE_CONNECTINON) < 0)
    {
        m_Logger.log(LogLevel::Error, "{}:Listen failed!",__func__);
        logLastError(m_Logger);
        return false;
    }
    m_Logger.log(LogLevel::Info, "{}:Server name:{} Port:{}",__func__, hostName, m_PortNumber);
    
    return true; 
}

bool ChatServer::getIsConnected() const
{
    return m_IsConnected.load();
}

void ChatServer::setIsConnected(bool isConnected) 
{
    m_IsConnected.store(isConnected);
}

void ChatServer::closeAllClientSockets() 
{
    m_Logger.log(LogLevel::Debug, "{}:All client sockets closed.",__func__);    
    lock_guard lock(m_Mutex);
    for (const auto &sd : m_ClientSockets){
        CLOSESOCKET(sd);
    }
    m_ClientSockets.clear();
}

bool ChatServer::getClientIP(int sd )
{
    char ip[INET6_ADDRSTRLEN];
    int port;
    struct sockaddr_storage sockAddr;
    socklen_t sockAddrLen = sizeof(sockAddr);
    memset(&sockAddr, 0, sizeof(sockAddr));
    const char *ipVer;
    if (getpeername(sd, (struct sockaddr *)&sockAddr, &sockAddrLen) != 0)
    {
        m_Logger.log(LogLevel::Error, "{}:Getpeername failed!",__func__);
        logLastError(m_Logger);
        return false;
    }
    
    if (sockAddr.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)&sockAddr;
        port = ntohs(s->sin_port);
        inet_ntop(AF_INET, &s->sin_addr, ip, sizeof(ip));
        ipVer = "IPV4:";
    } else { // AF_INET6
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&sockAddr;
        port = ntohs(s->sin6_port);
        inet_ntop(AF_INET6, &s->sin6_addr, ip, sizeof(ip));
        ipVer = "IPV6:";
    }
    m_Logger.log(LogLevel::Info, "{}:{}:{} Socket: {}",__func__,  ipVer, ip, port);
    return true;
}

bool ChatServer::sendMessage(int sd, const string_view message)
{
    string length_str = to_string(message.length());
    string padded_length_str = string(4 - length_str.length(), '0') + length_str;
    string full_message = padded_length_str + string(message);

    size_t bytesSent = send(sd, full_message.data(), full_message.length(), 0);
    if (bytesSent < 0)
    {
        m_Logger.log(LogLevel::Error, "{}:Send to client failed!",__func__);
        logLastError(m_Logger);
        return false;
    }
    return true;
}

void ChatServer::threadBroadcastMessage() {
    m_Logger.log(LogLevel::Debug, "{}: Broadcast thread started.", __func__);

    m_BroadcastThread = jthread([this](stop_token token) {
        while (!token.stop_requested()) {
            pair<int, string> front;

            {
                unique_lock lock(m_BroadcastMutex);
                // Wait until either a message arrives or stop is requested
                m_Cv.wait(lock, [&] {
                    return !m_BroadcastMessageQueue.empty() || token.stop_requested();
                });

                if (token.stop_requested()){
                    break;
                }
                    
                front = move(m_BroadcastMessageQueue.front());
                m_BroadcastMessageQueue.pop();
            }

            if (front.first > 0) {
                int sd = front.first;
                string message = move(front.second); 
                sendProdcastMessage(sd, message);
            }
        }
        m_Logger.log(LogLevel::Debug, "{}: Broadcast thread stopped", __func__);
    });
}

void ChatServer::sendProdcastMessage(int sd, const string& message){
    int tries = 0;
    while(tries < 5)
    {
        if (sendMessage(sd, message)) {
            //m_Logger.log(LogLevel::Debug, "{}: sd:{} Sent message to client:{}", __func__, sd, message);
            m_Logger.log(LogLevel::Debug, "{}: sd:{} Sent message to client", __func__, sd);
            break;
        } else {
            m_Logger.log(LogLevel::Error, "{}: Failed to send message to client:{}", __func__, sd);
            m_Logger.log(LogLevel::Info, "{}: Trying again in 0,5 seconds...", __func__);
            this_thread::sleep_for(chrono::milliseconds(500));
        }
        tries++;
    }
}
void ChatServer::addProadcastMessage(int sd, const string& message) {
    {
        lock_guard lock(m_BroadcastMutex);
        m_BroadcastMessageQueue.emplace(make_pair(sd, message));
    }
    m_Cv.notify_one();
}   

void ChatServer::closeSocket(int sd)
{
    CLOSESOCKET(sd);
    lock_guard lock(m_Mutex);
    m_ClientSockets.erase(sd);
    m_Logger.log(LogLevel::Debug, "{}:Socket closed.",__func__);
}

unordered_set<int> ChatServer::getClientSockets() const {
    return m_ClientSockets;
}

ChatServer::~ChatServer(){
    CLOSESOCKET(m_SockfdListener);
    setIsConnected(false);
    m_BroadcastThread.request_stop();
    m_Logger.log(LogLevel::Debug, "{}:ChatServer class destroyed.",__func__);
    m_Logger.setDone(true);
}