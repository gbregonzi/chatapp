#include <fcntl.h>
#include <netinet/in.h>
#include <memory>

#include "handleConnectionsLinux.h"

constexpr int WAITING_TIME = -1;

HandleConnectionsLinux::HandleConnectionsLinux(Logger &logger, const string& serverName, const string& portNumber):
                        ChatServer(logger, serverName, portNumber){
    m_epollFd = epoll_create1(0);
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = m_SockfdListener;
    epoll_ctl(m_epollFd, EPOLL_CTL_ADD, m_SockfdListener, &event);
    makeSocketNonBlocking(m_SockfdListener);
    size_t threadCount = thread::hardware_concurrency();
    m_Logger.log(LogLevel::Info, "{}:Starting thread poll", __func__);
    threadPool = make_unique<ThreadPool>(threadCount); 
}

HandleConnectionsLinux::~HandleConnectionsLinux(){
    close(m_SockfdListener);
    setIsConnected(false);
    m_BroadcastThread.request_stop();
    m_Logger.log(LogLevel::Debug, "{}:HandleConnectionsLinux class destroyed.",__func__);
    m_Logger.setDone(true);
}

void HandleConnectionsLinux::handleClient(int clientFd){
    char buffer[BUFFER_SIZE];
    int bytesRead = read(clientFd, buffer, sizeof(buffer));
    if (bytesRead > 0) {
        for (auto& sd:m_ClientSockets){
            if (sd != clientFd){
                lock_guard lock(m_Mutex);
                buffer[bytesRead] = 0x0;
                m_BroadcastMessageQueue.emplace(make_pair(sd, buffer));    
            }    
        }
    }
    if (bytesRead == 0){
        epoll_ctl(m_epollFd, EPOLL_CTL_DEL, clientFd, nullptr);
        m_Logger.log(LogLevel::Info, "{}:Client desconnected", __func__);
        closeSocket(clientFd);
    }   
}
   
void HandleConnectionsLinux::acceptConnections(){
    m_Logger.log(LogLevel::Info, "{}:Accept Connections start", __func__);

    while (getIsConnected()) {
        int nfds = epoll_wait(m_epollFd, m_Events, MAX_QUEUE_CONNECTINON, WAITING_TIME);
        struct epoll_event event;
        int clientFd;
        for (int i = 0; i < nfds; ++i) {
            if (m_Events[i].data.fd == m_SockfdListener) {
                clientFd = accept(m_SockfdListener, NULL, NULL);
                event.events = EPOLLIN;
                event.data.fd = clientFd;
                epoll_ctl(m_epollFd, EPOLL_CTL_ADD, clientFd, &event);
            } else {
                clientFd = m_Events[i].data.fd;
                getClientIP(clientFd);
                makeSocketNonBlocking(clientFd);

                threadPool->enqueue([&clientFd, this] {handleClient(clientFd); });
            }
        }
    }
}

int HandleConnectionsLinux::makeSocketNonBlocking(int sfd) {
    int flags = fcntl(sfd, F_GETFL, 0);
    return fcntl(sfd, F_SETFL, flags | O_NONBLOCK);
}

    
