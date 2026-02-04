#include <iostream>
#include <string>
#include <thread>
#include <atomic>

#include "clientSocket.h"
#include "../utils/file.h"
using namespace std;

constexpr int FAILURE = -1;
jthread sendThread;
jthread readThread;
atomic<int> messageCount{0};
// void sendMessageThread(ClientSocket& clientSocket, atomic<bool>& chatActive)
// {
//     cout << "Started send message thread." << "\n";
//     while (chatActive.load())
//     {
//         string message;
//         cout << "Enter message to send (type 'quit' to exit): ";
//         getline(cin, message);
//         if (message == "quit" || message == "q" || message.empty())
//         {
//             break;
//         }
//         // Check for optional repeat message count
//         string temp = message.find(",") != string::npos ? message.substr(0, message.find(",")) : "0";
//         if (stoi(temp) > 0)
//         {
//             message = message.substr(temp.length() + 1, message.length() - (temp.length() + 1));
//         }
//         else{
//             temp = "1";
//         }
//         for (int i=0; i < stoi(temp); i++)
//         {
//             int bytesSent = clientSocket.sendMessage(message);
//             if (bytesSent == FAILURE){
//                 break;
//             }
//         }
//     }
//     cout << "Exiting send message thread." << "\n"; 
//     chatActive.store(false);
//     clientSocket.socketClosed();
// }
bool OpenFile(path m_path, ifstream& is) {
	try {
		is.open(m_path.string(), ios::in | ios::binary);

		if (!is.is_open()) {
			cout << "Unable to open file:" + m_path.string() << GetLastError() << "\n";
			return false;
		}
		is.seekg(0);
	}
	catch (const exception& ex) {
		cout << ex.what() << "\n";
		cout << ex.what() << GetLastError();
		return false;
	}
	return true;
}
void readData(ifstream& is, string &buffer)
{
	char ch{ 0x0 };
	while (is.get(ch)) {
		if (ch == 0x0d) {
			is.get(ch);
			break;
		}
		else {
			buffer += ch;
		}
	}
}


void sendMessageThread(ClientSocket& clientSocket, atomic<bool>& chatActive)
{
    cout << "Started send message thread." << "\n";
    string fileName = "send_messages.txt";
    path path = current_path() / "log" / fileName.data();
    ifstream is;
    while (true)
    {
        if (!OpenFile(path, is)) {
            cout << "Failed to open " << path.string() << " file. Proceeding with console input.\n";
            return;
        } 
        else {
            cout << "Loading messages from " << path.string() << " file." << "\n";
            while (!is.eof()) {
                string message;
                readData(is, message);
                if (message.empty()) {
                    continue;
                }
                // Check for optional repeat message count
                string temp = message.find(",") != string::npos ? message.substr(0, message.find(",")) : "0";
                if (stoi(temp) > 0)
                {
                    messageCount = stoi(temp);
                    message = message.substr(temp.length() + 1, message.length() - (temp.length() + 1));
                }
                else{
                    messageCount = 1;
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
            is.close();
        }

        // string message;
        // cout << "Repeat send message (type 'yes' or 'no'): ";
        // getline(cin, message);
        // if (message == "no" || message == "q" || message.empty())
        // {
        //     break;
        // }
        break;
    }
    cout << "Exiting send message thread." << "\n"; 
    while(messageCount.load() > 0){
        this_thread::sleep_for(chrono::milliseconds(100));
    }   
    //clientSocket.socketClosed();
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
        messageCount--;
    }
    cout << "Exiting read message thread." << "\n"; 
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