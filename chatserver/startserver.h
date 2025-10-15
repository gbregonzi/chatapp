#pragma once
#include <string>
#include <thread>
#include <mutex>
#include "chatserver.h"
#include "../utils/outputStream.h"

using namespace std;

class StartServer {
private:
    jthread readFromClientThread;
    jthread serverBroadcastThread;
    unique_ptr<OutputStream> m_Cout;
    unique_ptr<ServerSocket> m_ServerSocket;
    string serverName{};
    string portNumber{};
public:
    StartServer(const string& serverName, const string& portNumber);
    //void sendBroadcastTextMessage();
    //void readFromClient(int clientSocket);
    //void startReadFromClientThread(int clientSocket);
    //void listenClientConnections();
    int Run();
};
struct ServerSocketFactory{
    inline static unique_ptr<ServerSocket> create(unique_ptr<OutputStream> &outputStream, 
                                            const string& serverName, const string& portNumber){
        return make_unique<ServerSocket>(outputStream, serverName, portNumber);
    };
};
