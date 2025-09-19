#include <iostream>
#include <thread>
#include <iostream>
#include "chatserver.h"
#include "../utils/outputStream.h"

using namespace std;

class joiner {
    std::thread& t;
public:
    explicit joiner(std::thread& t_) : t(t_) {} 
    ~joiner() {
        if (t.joinable()) {
            t.join();
        }
    }   
};

class StartServer{
private:
    jthread readFromClientThread;
    jthread serverBroadcastThrea;
    OutputStream& m_cout;
    ServerSocket& m_ServerSocket;
public:
    StartServer(ServerSocket& m_ServerSocket, OutputStream& _cout);
    void sendBroadcastTextMessage();
    void readFromClient(int clientSocket);
    void startFromClientThread(int clientSocket);
    void listenClientConnections();
};

StartServer::StartServer(ServerSocket& _ServerSocket, OutputStream& _cout): m_ServerSocket(_ServerSocket) ,m_cout(_cout){};

void StartServer::sendBroadcastTextMessage()
{
    m_cout << __func__ << ":" << "Thread started." << endl; 
    serverBroadcastThrea = jthread([this]() {
        string message;
        
        while (m_ServerSocket.getIsConnected())
        {
            m_cout << "Enter message to send to client (or 'quit' to exit): ";
            getline(cin, message);
            if (message == "quit")
            {
                m_ServerSocket.setIsConnected(false);
                m_ServerSocket.closeSocketServer();
                m_ServerSocket.closeAllClientSockets();
                break;
            }
            m_ServerSocket.addBroadcastTextMessage(message);
        }
        m_cout << "sendBroadcastTextMessage:" << "Thread stopping..." << endl;
    });
}

void StartServer::readFromClient(int clientSocket)
{
    m_cout << __func__ << ":" << "Read thread started for client: " << clientSocket << endl;  
    while (m_ServerSocket.getIsConnected())
    {
        string message;
        size_t bytesRead = m_ServerSocket.readMessage(message, clientSocket);
        if (bytesRead < 0)
        {
            m_cout << __func__ << ":" << ":" << "Read from client failed!" << endl;
            m_ServerSocket.logErrorMessage(errno);
            break;
        }
        else if (bytesRead == 0)
        {
            m_cout << __func__ << ":" << "Client disconnected." << endl;
            break;
        }
        if (message == "exit")
        {
            m_cout << __func__ << ":" << "Client requested to end chat." << endl;
            break;
        }

        for (const auto &sd : m_ServerSocket.getClientSockets()){
            if (sd != clientSocket) { // Avoid sending the message back to the sender
                m_cout << __func__ << ":" << "sd: " << sd << endl;
                m_ServerSocket.addBroadcastTextMessage(message, sd);
            }
        }
    }
    m_ServerSocket.closeSocket(clientSocket);
    m_cout << __func__ << ":" << "Read thread stopping..." << endl;
}

void StartServer::startFromClientThread(int clientSocket){
    readFromClientThread = jthread([this, clientSocket](stop_token token) {
        readFromClient(clientSocket);
    });
}

void StartServer::listenClientConnections()
{
    m_cout << __func__ << ":" << "Listening for client connections..." << endl;
    while (m_ServerSocket.getIsConnected())
    {
        int clientSocket = m_ServerSocket.handleConnections();
        if (clientSocket > 0)
        {
            startFromClientThread(clientSocket); 
        }
    }
    serverBroadcastThrea.request_stop();
    readFromClientThread.request_stop();
    m_cout << __func__ << ":" << "Listen thread stopping..." << endl;
}

int main (int argc, const char* argv[]){
    OutputStream out(cout);
    ServerSocket serverSocket(out);
    StartServer startServer(serverSocket, out);
    if (serverSocket.getIsConnected()) {
        startServer.sendBroadcastTextMessage();
        startServer.listenClientConnections();
        out << __func__ << "Server is shutting down..." << endl;
    } else {
        out << __func__ << "Server failed to connect." << endl;
        return EXIT_FAILURE;
    }

    return 0;
}
