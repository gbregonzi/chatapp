#include "handleConnectionsLinux.h"


HandleConnectionsLinux::HandleConnectionsLinux(Logger &logger, const string& serverName, const string& portNumber){
    m_epollFd = epoll_create1(0);
    ev.events = EPOLLIN;
    ev.data.fd = m_SockfdListener;
    epoll_ctl(m_epollFd, EPOLL_CTL_ADD, m_SockfdListener, &ev);
}

HandleConnectionsLinux::~HandleConnectionsLinux(){
    
}

void HandleConnectionsLinux::workerThread(HANDLE iocp){

}

void HandleConnectionsLinux::associateSocket(uint64_t clientSocket){

}
   
void HandleConnectionsLinux::acceptConnections(){

}

bool HandleConnectionsLinux::createListner(){

}
    
