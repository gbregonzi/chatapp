#include "startServer.h"

StartServer::StartServer(const string& serverName, const string& portNumber)
    : serverName(serverName),
      portNumber(portNumber),
      m_Logger(LoggerFactory::getInstance()),
      m_ServerSocket(ServerSocketFactory::getInstance(m_Logger, serverName, portNumber))
{
}

int StartServer::Run(){
    if (m_ServerSocket.getIsConnected()) {
        m_ServerSocket.handleSelectConnections();    
        m_ServerSocket.closeAllClientSockets();
        m_Logger.log(LogLevel::Info, "{}:{}", __func__ , "Server is shutting down...");
        return EXIT_SUCCESS;
    }
    m_Logger.log(LogLevel::Error, "{}:{}", __func__ ,"Server failed to connect.");
    return EXIT_FAILURE;
}  
