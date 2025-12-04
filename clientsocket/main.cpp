#include <iostream>
#include <string>
#include <thread>
#include <atomic>

#include "clientSocket.h"
using namespace std;

constexpr int FAILURE = -1;
jthread sendThread;
jthread readThread;

void sendMessageThread(ClientSocket& clientSocket, atomic<bool>& chatActive)
{
    cout << "Started send message thread." << "\n";
    while (chatActive.load())
    {
        string message;
        cout << "Enter message to send (type 'quit' to exit): ";
        getline(cin, message);
        if (message == "quit" || message == "q" || message.empty())
        {
            break;
        }
        string temp = message.find(",") != string::npos ? message.substr(0, message.find(",")) : "0";
        if (stoi(temp) > 0)
        {
            message = message.substr(temp.length() + 1, message.length() - (temp.length() + 1));
        }
        else{
            temp = "1";
        }
        for (int i=0; i < stoi(temp); i++)
        {
            int bytesSent = clientSocket.sendMessage(message);
            if (bytesSent == FAILURE){
                break;
            }
        }
    }
    cout << "Exiting send message thread." << "\n"; 
    chatActive.store(false);
    clientSocket.socketClosed();
}

void startSendMessageThread(ClientSocket& clientcocket, atomic<bool>& chatActive)
{
    sendThread = jthread(sendMessageThread, ref(clientcocket), ref(chatActive));
}

void readMessageThread(ClientSocket& clientcocket, atomic<bool>& chatActive)
{
    cout << "Started read message thread." << "\n";
    
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
    cout << "Exiting read message thread." << "\n"; 
    chatActive.store(false);
    clientcocket.socketClosed();
}

void startReadMessageThread(ClientSocket& clientcocket, atomic<bool>& chatActive)
{
    readThread = jthread(readMessageThread, ref(clientcocket), ref(chatActive));
}

int main (int argc, char *argv[]) {
    atomic<bool> chatActive{true};
    string logFileName;

    if (argc != 4) {
        cerr << __func__ << ":Usage: " << argv[0] << " <server:ip> <server_port> <log_file_name>" << std::endl;
        return 1;
    }
    
    if (argc < 4){
        cout << __func__ << ":The file name wasn't provide, Using default \"clientSocket.log\"" << "\n";
        logFileName = "clientSocket.log";
    }
    else {
            logFileName = string(argv[3]);
    }

    string server_ip = argv[1];
    const char *portHostName = argv[2];
    cout << __func__ << ":Server IP: " << server_ip << ", Port: " << portHostName << "Log File Name:" << logFileName << "\n"; 
    ClientSocket& client = ClientSocketFactory::getInstance(server_ip, portHostName, logFileName);
    if (client.connect() == 0 && client.getLogggerFileOpen()) {
        cout << __func__ << ":Connected to server successfully!" << "\n";
        startReadMessageThread(client, chatActive);
        startSendMessageThread(client, chatActive);
    } else {
        cout << __func__ << ":Failed to connect to server." << "\n";
        chatActive.store(false);
        return EXIT_FAILURE;
    }
    while(chatActive.load()) {
        this_thread::sleep_for(chrono::milliseconds(100));
    }   
    cout << __func__ << ":Chat ended. Exiting program." << "\n";
    return 0;
}