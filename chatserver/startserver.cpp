#include "startserver.h"

unique_ptr<ServerSocket> ServerSocketFactory::create(unique_ptr<OutputStream> &outputStream, const string& serverName, const string& portNumber){
    return make_unique<ServerSocket>(outputStream, serverName, portNumber);
}


std::unique_ptr<OutputStream> LogFactory::create(mutex& _mutex, ostream& os) {
    return make_unique<OutputStream>(_mutex, os);
}    

StartServer::StartServer(const string& serverName, const string& portNumber): serverName(serverName), portNumber(portNumber) {
    m_cout = LogFactory::create(m_mutex);
    m_ServerSocket = ServerSocketFactory::create(m_cout, serverName, portNumber);
};

int StartServer::Run(){
    if (m_ServerSocket->getIsConnected()) {
        m_ServerSocket->handleSelectConnections();
        // sendBroadcastTextMessage();
        // listenClientConnections();
        *m_cout << __func__ << ":Server is shutting down...\n";
    } else {
        *m_cout << __func__ << "Server failed to connect.\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}  

void StartServer::sendBroadcastTextMessage()
{
    m_cout->log("sendBroadcastTextMessage:Thread started."); 
    serverBroadcastThread = jthread([this]() {
        string message;
        
        while (m_ServerSocket->getIsConnected())
        {
           m_cout->log("Enter message to send to client (or 'quit' to exit)");
            getline(cin, message);
            if (message == "quit")
            {
                m_ServerSocket->setIsConnected(false);
                m_ServerSocket->closeSocketServer();
                m_ServerSocket->closeAllClientSockets();
                break;
            }

            {
                lock_guard<mutex> lock(m_ServerSocket->getMutex());
                m_ServerSocket->addBroadcastTextMessage(message);
            }
        }
        m_cout->log("sendBroadcastTextMessage: Thread stopping...");
    });
}

void StartServer::readFromClient(int clientSocket)
{
    *m_cout << __func__ << ":" << "Read thread started for client: " << clientSocket << "\n";  
    while (m_ServerSocket->getIsConnected())
    {
        string message;
        size_t bytesRead = m_ServerSocket->readMessage(message, clientSocket);
        if (bytesRead < 0)
        {
            *m_cout << __func__ << ":" << ":" << "Read from client failed!\n";
            m_ServerSocket->logErrorMessage(errno);
            break;
        }
        else if (bytesRead == 0)
        {
            m_cout->log("readFromClient:Client disconnected.");
            break;
        }
        if (message == "exit")
        {
            *m_cout << __func__ << ":" << "Client requested to end chat.\n";
            break;
        }
        
        {
            lock_guard<mutex> lock(m_ServerSocket->getMutex());
            for (const auto &sd : m_ServerSocket->getClientSockets()){
                if (sd != clientSocket) { // Avoid sending the message back to the sender
                    *m_cout << __func__ << ":" << "sd: " << sd << "\n";
                    m_ServerSocket->addBroadcastTextMessage(message, sd);
                }
            }
        }
    }
    m_ServerSocket->closeSocket(clientSocket);
    *m_cout << __func__ << ":" << "Read thread stopping...\n";
}

void StartServer::startReadFromClientThread(int clientSocket){
    readFromClientThread = jthread([this, clientSocket](stop_token token) {
        readFromClient(clientSocket);

    });
}

void StartServer::listenClientConnections()
{
    *m_cout << __func__ << ":" << "Listening for client connections...\n";
    while (m_ServerSocket->getIsConnected())
    {
        int clientSocket = m_ServerSocket->handleConnections();
        if (clientSocket > 0)
        {
            startReadFromClientThread(clientSocket); 
        }
    }
    serverBroadcastThread.request_stop();
    readFromClientThread.request_stop();
    *m_cout << __func__ << ":" << "Listen thread stopping...\n";
}
