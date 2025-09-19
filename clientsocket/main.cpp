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
        int bytesSent = clientSocket.sendMessage(message); 
        if (message == "quit" || bytesSent == FAILURE)
        {
            chatActive.store(false);
            break;
        }
    }
}

void startSendMessageThread(ClientSocket& clientcocket, atomic<bool>& chatActive)
{
    thread sendThread(sendMessageThread, ref(clientcocket), ref(chatActive));
    sendThread.detach(); // Detach the thread to run independently
}

void readMessageThread(ClientSocket& clientcocket, atomic<bool>& chatActive)
{
    while (chatActive.load())
    {
        string message;
        ssize_t bytesRead = clientcocket.readMessage(message); 
        if (bytesRead == FAILURE || message == "quit")
        {
            break;
        }
    }
    chatActive.store(false);
    clientcocket.SocketClosed();
}

void startReadMessageThread(ClientSocket& clientcocket, atomic<bool>& chatActive)
{
    thread sendThread(readMessageThread, ref(clientcocket), ref(chatActive));
    sendThread.detach(); // Detach the thread to run independently
}

int main (int argc, char *argv[]) {
    atomic<bool> chatActive{true};

    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <server:ip> <server_port>" << std::endl;
        return 1;
    }

    string server_ip = argv[1];
    int server_port = stoi(argv[2]);
    cout << "Server IP: " << server_ip << ", Server Port: " << server_port << endl; 
    ClientSocket client(server_ip, server_port);
    if (client.connect() == 0) {
        cout << "Connected to server successfully!" << endl;
        startSendMessageThread(client, chatActive);
        startReadMessageThread(client, chatActive);
    } else {
        cout << "Failed to connect to server." << endl;
        chatActive.store(false);
        return EXIT_FAILURE;
    }
    while(chatActive.load()) {
        this_thread::sleep_for(chrono::milliseconds(100));
    }   

    return 0;
}