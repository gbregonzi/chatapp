#include <iostream>
#include <thread>
#include "chatserver.h"

using namespace std;

void sendBroadcastTextMessage(ServerSocket& serverSocket)
{
    thread serverBroadcastThread([&serverSocket]() {
        string message;
        
        while (serverSocket.getIsConnected())
        {
            cout << " Enter message to send to client (or 'quit' to exit): ";
            getline(cin, message);
            if (message == "quit")
            {
                serverSocket.setIsConnected(false);
                serverSocket.SocketClosed(serverSocket.getServerSocket());
                break;
            }
            serverSocket.addBroadcastTextMessage(message);
        }
    });
    serverBroadcastThread.detach();
}

void readFromClient(ServerSocket& serverSocket, int clientSocket)
{
    while (true)
    {
        string message;
        size_t bytesRead = serverSocket.readMessage(message, clientSocket);
        if (bytesRead < 0)
        {
            cerr << __func__ << ":" << ":" << "Read from client failed!" << endl;
            serverSocket.logErrorMessage(errno);
            break;
        }
        else if (bytesRead == 0)
        {
            cout << __func__ << ":" << "Client disconnected." << endl;
            break;
        }
        if (message == "exit")
        {
            cout << __func__ << ":" << "Client requested to end chat." << endl;
            break;
        }
        for (const auto &sd : serverSocket.getClientSockets()){
            if (sd != clientSocket) { // Avoid sending the message back to the sender
                cout << __func__ << ":" << "sd: " << sd << endl;
                serverSocket.addBroadcastTextMessage(message);
            }
        }
    }
    serverSocket.SocketClosed(clientSocket);
}

void readFromClientThread(ServerSocket& serverSocket, int clientSocket)
{
    thread readThread(readFromClient, ref(serverSocket), clientSocket);
    readThread.detach();
}
void listenClientConnections(ServerSocket& serverSocket)
{
    while (serverSocket.getIsConnected())
    {
        int clientSocket = serverSocket.listenClientConnections();
        if (serverSocket.getIsConnected() == false)
        {
            return;
        }
        if (clientSocket < 0)
        {
            continue;
        }
        readFromClientThread(serverSocket, clientSocket); 
    }
}

int main (int argc, const char* argv[]){
    ServerSocket serverSocket;
    if (serverSocket.getIsConnected()) {
        sendBroadcastTextMessage(serverSocket);
        listenClientConnections(serverSocket);
    } else {
        cerr << "Server failed to connect." << endl;
        return EXIT_FAILURE;
    }

    return 0;
}
