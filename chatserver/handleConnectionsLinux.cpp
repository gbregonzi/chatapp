#include "handleConnectionsLinux.h"

constexpr int WAITING_TIME = -1;

HandleConnectionsLinux::HandleConnectionsLinux(Logger &logger, const string& serverName, const string& portNumber):
                        ChatServer(logger, serverName, portNumber){
    m_epollFd = epoll_create1(0);
    ev.events = EPOLLIN;
    ev.data.fd = m_SockfdListener;
    epoll_ctl(m_epollFd, EPOLL_CTL_ADD, m_SockfdListener, &ev);
}

HandleConnectionsLinux::~HandleConnectionsLinux(){
    close(m_SockfdListener);
    setIsConnected(false);
    m_BroadcastThread.request_stop();
    m_Logger.log(LogLevel::Debug, "{}:HandleConnectionsLinux class destroyed.",__func__);
    m_Logger.setDone(true);
}

void HandleConnectionsLinux::workerThread(HANDLE iocp){

}

void HandleConnectionsLinux::associateSocket(uint64_t clientSocket){

}
   
void HandleConnectionsLinux::acceptConnections(){
    while (getIsConnected()) {
        int nfds = epoll_wait(m_epollFd, events, MAX_EVENTS, WAITING_TIME);
        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == m_SockfdListener) {
                int client_fd = accept(m_SockfdListener, NULL, NULL);
                ev.events = EPOLLIN;
                ev.data.fd = client_fd;
                epoll_ctl(m_epollFd, EPOLL_CTL_ADD, client_fd, &ev);
            } else {
                char buffer[1024] = {0};
                int bytes = read(events[i].data.fd, buffer, sizeof(buffer));
                if (bytes <= 0) {
                    close(events[i].data.fd);
                } else {
                    send(events[i].data.fd, buffer, bytes, 0); // Echo back
                }
            }
        }
    }
}

    
