#include <iostream>
#include <string>
#include <thread>
#include <atomic>

#include "clientSocket_new.h"
#include "../utils/util.h"
#include "../utils/file.h"

using namespace std;

constexpr int FAILURE = -1;
vector<jthread> sendThread;
vector<jthread> readThread;

void sendMessageThread(ClientSocket& clientSocket, Logger& logger)
{
    cout << "Started send message thread." << "\n";
    string fileName = "send_messages.txt";
    path path = current_path() / "log" / fileName.data();
    File file(logger, path.string());
    if (!file.OpenFile()) {
        cout << "Failed to open" << path.string() << " file. Proceeding with console input.\n";
    } else {
        cout << "Loading messages from " << path.string() << " file." << "\n";
        while (!file.eof()) {
            string message;
            file.readData(message);
            if (message.empty()) {
                continue;
            }
            // Check for optional repeat message count
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
                if (bytesSent == FAILURE)
                {
                    break;
                }
            }
        }
        cout << "Finished sending messages from file." << "\n";
        file.closeInputFile();
        clientSocket.socketClosed();
        return;
    }

    while (true)
    {
        string message;
        cout << "Enter message to send (type 'quit' to exit): ";
        getline(cin, message);
        if (message == "quit" || message == "q" || message.empty())
        {
            break;
        }
        // Check for optional repeat message count
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
    clientSocket.socketClosed();
}

future<void> startSendMessageThread(ClientSocket& clientcocket, Logger& logger)
{
    promise<void> prom;
    future<void> fut = prom.get_future();
    sendThread.push_back(jthread([&clientcocket, p = move(prom), &logger]() mutable {
        sendMessageThread(clientcocket, logger);
        p.set_value(); // notify completion
    }));
    return fut;
}

void readMessageThread(ClientSocket& clientcocket)
{
    cout << "Started read message thread." << "\n";
    
    while (true)
    {
        string message;
        size_t bytesRead = clientcocket.readMessage(message); 
        if (bytesRead == FAILURE || message == "quit")
        {
            break;
        }
    }
    cout << "Exiting read message thread." << "\n"; 
    clientcocket.socketClosed();
}

future<void> startReadMessageThread(ClientSocket& clientSocket)
{
    promise<void> prom;
    future<void> fut = prom.get_future();
    readThread.push_back(jthread([&clientSocket, p = move(prom)]() mutable {
        readMessageThread(clientSocket);
        p.set_value(); // notify completion
    }));
    return fut;
}

int main (int argc, char *argv[]) {
    string logFileName;
    int clientInstance;

    if (argc != 5) {
        cerr << __func__ << ":Usage: " << argv[0] << "<Client instance> <server name or ip> <server_port> <log_file_name>" << std::endl;
        return 1;
    }
    
    if (argc < 5){
        cout << __func__ << ":The file name wasn't provide, Using default \"clientSocket.log\"" << "\n";
        logFileName = "clientSocket";
        clientInstance = 1;
    }
    else {
            logFileName = string(argv[4]);
            clientInstance = stoi(argv[1]);
    }

    string server_ip = argv[2];
    const char *portHostName = argv[3];
    vector<reference_wrapper<ClientSocket>> client;
    vector<future<void>> readPromise;
    vector<future<void>> sendPromise;
    for (int i = 0; i < clientInstance; i++)
    {
        string logFileNameInstance = logFileName + "_" + to_string(i+1) + ".log";
        cout << __func__ << ":Server IP: " << server_ip << ", Port: " << portHostName << ", Log File Name:" << logFileNameInstance << "\n";
        client.push_back(ClientSocketFactory::getInstance(server_ip, portHostName, logFileNameInstance));
    }
    
    for (size_t i = 0; i < client.size(); i++)
    {
        if (client[i].get().connect() == EXIT_SUCCESS && client[i].get().getLogggerFileOpen()) {
            cout << __func__ << ":Connected to server successfully!" << "\n";
            readPromise.emplace_back(startReadMessageThread(client[i].get()));
            sendPromise.emplace_back(startSendMessageThread(client[i].get(), client[i].get().getLogger()));
        } else {
            cout << __func__ << ":Failed to connect to server." << "\n";
            //chatActive.store(false);
            return EXIT_FAILURE;
        }
    }
    for (future<void>& ft : readPromise) 
    {
        ft.get(); // wait for read thread to complete
    }   
    
    cout << __func__ << ":Chat ended. Exiting program." << "\n";
    return 0;
}