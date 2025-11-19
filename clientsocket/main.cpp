#include <iostream>
#include <string>
#include <thread>
#include <atomic>

#include "clientSocket.h"
using namespace std;

constexpr int FAILURE = -1;


void sendMessageThread(ClientSocket& clientSocket, atomic<bool>& chatActive)
{
    while (chatActive.load())
    {
        string message;
        cout << "Enter message to send (type 'quit' to exit): ";
        getline(cin, message);
        if (message == "quit")
        {
            break;
        }
        int bytesSent = clientSocket.sendMessage(message); 
        if (bytesSent == FAILURE){
            break;
        }
    }
    chatActive.store(false);
}

void startSendMessageThread(ClientSocket& clientcocket, atomic<bool>& chatActive)
{
    jthread sendThread(sendMessageThread, ref(clientcocket), ref(chatActive));
}

void readMessageThread(ClientSocket& clientcocket, atomic<bool>& chatActive)
{
    while (chatActive.load())
    {
        string message;
        size_t bytesRead = clientcocket.readMessage(message); 
        if (bytesRead == FAILURE || message == "quit")
        {
            chatActive.store(false);
            break;
        }
    }
    
    clientcocket.SocketClosed();
}

void startReadMessageThread(ClientSocket& clientcocket, atomic<bool>& chatActive)
{
    jthread sendThread(readMessageThread, ref(clientcocket), ref(chatActive));
}

int main (int argc, char *argv[]) {
    atomic<bool> chatActive{true};
    string logFileName;
    if (argc != 4) {
        cerr << "Usage: " << argv[0] << " <server:ip> <server_port> <log_file_name>" << std::endl;
        return 1;
    }
    
    if (argc < 4){
        cout << "The file name wasn't provide, Using default \"clientSocket.log\"" << "\n";
        logFileName = "clientSocket.log";
    }
    else {
            logFileName = string(argv[3]);
    }

    string server_ip = argv[1];
    const char *portHostName = argv[2];
    cout << "Server IP: " << server_ip << ", Port: " << portHostName << "Log File Name:" << logFileName << "\n"; 
    ClientSocket& client = ClientSocketFactory::getInstance(server_ip, portHostName, logFileName);
    if (client.connect() == 0) {
        cout << "Connected to server successfully!" << "\n";
        startSendMessageThread(client, chatActive);
        startReadMessageThread(client, chatActive);
    } else {
        cout << "Failed to connect to server." << "\n";
        chatActive.store(false);
        return EXIT_FAILURE;
    }
    while(chatActive.load()) {
        this_thread::sleep_for(chrono::milliseconds(100));
    }   

    return 0;
}