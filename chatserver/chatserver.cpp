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

#include "ChatServer.h"

using namespace std;

ChatServer::ChatServer(Logger& logger, const string& serverName, const string& port): 
                          m_Logger(logger), m_ServerName(serverName), m_PortNumber(port)   
{
    m_IsConnected.store(true);
    threadBroadcastMessage();
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
    lock_guard lock(m_Mutex);
    for (const auto &sd : m_ClientSockets){
        CLOSESOCKET(sd);
    }
    m_ClientSockets.clear();
    m_Logger.log(LogLevel::Debug, "{}:All client sockets closed.",__func__);    
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
        m_Logger.log(LogLevel::Error, "{}:Getpeername failed! Error: {}",__func__, errno);
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
    size_t bytesSent = send(sd, message.data(), message.length(), 0);
    if (bytesSent < 0)
    {
        m_Logger.log(LogLevel::Error, "{}:Send to client failed!",__func__);
        m_Logger.log(LogLevel::Info, "{}:Error Code:{}", __func__, ERROR_CODE);
        return false;
    }
    return true;
}

void ChatServer::threadBroadcastMessage() {
    m_Logger.log(LogLevel::Debug, "{}:Broadcast thread started.", __func__);

    m_BroadcastThread = jthread([this](stop_token token) {
        while (!token.stop_requested()) {
            pair<int, string> front;
            {
                lock_guard lock(m_Mutex);
                if (!m_BroadcastMessageQueue.empty()) {
                    front = m_BroadcastMessageQueue.front();
                    m_BroadcastMessageQueue.pop();
                }
            }
            
            if (front.first > 0) {
                int sd = front.first;
                const string& message = front.second;
                if (sendMessage(sd, message)) {
                    m_Logger.log(LogLevel::Debug, "{}:sd:{} Sent message to client:{}",__func__, sd, message);
                } else {
                    m_Logger.log(LogLevel::Error, "{}:Failed to send message to client:{}", __func__, sd);
                }
            }
            this_thread::yield;
        }
        m_Logger.log(LogLevel::Debug, "{}:Broadcast thread stoped", __func__);
    });
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

