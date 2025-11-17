#include "startServer.h"
#include "chatServerFactory.h"

StartServer::StartServer(const string& logFileName, const string& serverName, const string& portNumber)
    : serverName(serverName),
      portNumber(portNumber),
      m_Logger(LoggerFactory::getInstance(logFileName)),
      m_chatServer(chatServerFactory::getInstance(m_Logger, serverName, portNumber))
{
}

int StartServer::Run(){
    if (m_chatServer->getIsConnected()) {
        if (m_chatServer->createListner()){
            m_chatServer->AcceptConnections();    
        }
        m_chatServer->closeAllClientSockets();
        m_Logger.log(LogLevel::Info, "{}:{}", __func__ , "Server is shutting down...");
        return EXIT_SUCCESS;
    }
    m_Logger.log(LogLevel::Error, "{}:{}", __func__ ,"Server failed to connect.");
    return EXIT_FAILURE;
}  
