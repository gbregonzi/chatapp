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
            cout << "Enter message to send to client (or 'exit' to quit): ";
            getline(cin, message);
            if (message == "exit")
            {
                serverSocket.setIsConnected(false);
                break;
            }
            serverSocket.addBroadcastTextMessage(message);
        }
    });
    serverBroadcastThread.detach();
}

void readFromClient(ServerSocket& serverSocket, int sd)
{
    char buffer[BUFFER_SIZE];
    while (true)
    {
        string message;
        ssize_t bytesRead = serverSocket.readMessage(message);
        if (bytesRead < 0)
        {
            cerr << "Read from client failed!" << endl;
            serverSocket.getError(errno);
            break;
        }
        else if (bytesRead == 0)
        {
            cout << "Client disconnected." << endl;
            break;
        }
        if (strcmp(buffer, "exit") == 0)
        {
            cout << "Client requested to end chat." << endl;
            break;
        }
        serverSocket.getClientIP(sd);
        cout << "Received from client: " << buffer << endl;
        serverSocket.addBroadcastTextMessage(message);
    }
    serverSocket.setIsConnected(false);
    serverSocket.closeSocket(sd);
    cout << "Chat ended. Client socket closed." << endl;
}

void readFromClientThread(ServerSocket& serverSocket, int sd)
{
    thread readThread(readFromClient, ref(serverSocket), sd);
    readThread.detach();
}

int main (int argc, const char* argv[]){
    ServerSocket serverSocket;
    if (serverSocket.getIsConnected()) {
        serverSocket.listenClientConnections();
        readFromClientThread(serverSocket, 0); // Placeholder for actual client socket descriptor
        sendBroadcastTextMessage(serverSocket);
    } else {
        cerr << "Server failed to connect." << endl;
        return EXIT_FAILURE;
    }

    return 0;
}
