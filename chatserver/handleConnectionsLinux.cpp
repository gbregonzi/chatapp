#include <fcntl.h>
#include <netinet/in.h>
#include <memory>

#include "handleConnectionsLinux.h"

constexpr int WAITING_TIME = -1;
constexpr int SUCCESS = 0;
constexpr int FAILURE = -1;

HandleConnectionsLinux::HandleConnectionsLinux(Logger &logger, const string& serverName, const string& portNumber):
                        ChatServer(logger, serverName, portNumber){
    m_epollFd = epoll_create1(0);
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLOUT | EPOLLET;
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
    stringstream threadId;
    threadId << this_thread::get_id();
    m_Logger.log(LogLevel::Debug, "{}:Thread ID: {}", __func__, threadId.str());
    char buffer[BUFFER_SIZE];
    int bytesRead = read(clientFd, buffer, sizeof(buffer));
    if (bytesRead > 0) {
        lock_guard lock(m_Mutex);
        for (auto& sd:m_ClientSockets){
            if (sd != clientFd){
                buffer[bytesRead] = 0x0;
                addProadcastMessage(sd, buffer);
            }    
        }
    }
    if (bytesRead == 0){
        epoll_ctl(m_epollFd, EPOLL_CTL_DEL, clientFd, nullptr);
        m_Logger.log(LogLevel::Info, "{}:Client desconnected", __func__);
        closeSocket(clientFd);
    }   
}

int HandleConnectionsLinux::createEpollInstance(){
    m_epollFd = epoll_create1(0);
    if (m_epollFd == -1) {
        m_Logger.log(LogLevel::Error, "{}:Failed to create epoll file descriptor.", __func__);
        setIsConnected(false);
        return FAILURE;
    }   
    struct epoll_event event;
    event.events = EPOLLIN;// | EPOLLOUT | EPOLLET;
    event.data.fd = m_SockfdListener;
    if (epoll_ctl(m_epollFd, EPOLL_CTL_ADD, m_SockfdListener, &event) == FAILURE) {
        m_Logger.log(LogLevel::Error, "{}:Failed to add listener socket to epoll.", __func__);
        setIsConnected(false);
        close(m_epollFd);
        closeSocket(m_SockfdListener);
        return FAILURE;
    }
    makeSocketNonBlocking(m_SockfdListener);
    size_t threadCount = thread::hardware_concurrency();
    m_Logger.log(LogLevel::Info, "{}:Starting thread poll", __func__);
    threadPool = make_unique<ThreadPool>(threadCount); 
    return SUCCESS;
}

void HandleConnectionsLinux::acceptConnections(){
    m_Logger.log(LogLevel::Info, "{}:Accept Connections start", __func__);
    if (createEpollInstance() == FAILURE){
        m_Logger.log(LogLevel::Error, "{}:Epoll instance creation failed.", __func__);
        return;
    }

    while (getIsConnected()) {
        int nfds = epoll_wait(m_epollFd, m_Events, MAX_QUEUE_CONNECTINON, WAITING_TIME);
        if (nfds == -1) {
            m_Logger.log(LogLevel::Error, "{}:Failed to wait for events.", __func__);
            break;
        }
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
                m_Logger.log(LogLevel::Info, "{}:Client connected. Socket fd: {}", __func__, clientFd); 
                {
                    lock_guard lock(m_Mutex);   
                    m_ClientSockets.emplace(clientFd);
                }

                threadPool->enqueue([clientFd, this] {handleClient(clientFd); });
            }
        }
    }
    close(m_epollFd);
    m_Logger.log(LogLevel::Info, "{}:Stopped accepting connections.", __func__);
}

int HandleConnectionsLinux::makeSocketNonBlocking(int sfd) {
    int flags = fcntl(sfd, F_GETFL, 0);
    return fcntl(sfd, F_SETFL, flags | O_NONBLOCK);
}

    
