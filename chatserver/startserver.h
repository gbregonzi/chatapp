#pragma once
#include <string>
#include <thread>
#include <mutex>
#include "chatserver.h"
#include "../utils/outputStream.h"

using namespace std;

class ServerSocketFactory{
public:
    static unique_ptr<ServerSocket> create(unique_ptr<OutputStream> &outputStream, const string& serverName, const string& portNumber);
};

class LogFactory{
public:
    static unique_ptr<OutputStream>create(mutex& mt, ostream& os = cout); 
};

class StartServer {
private:
    jthread readFromClientThread;
    jthread serverBroadcastThread;
    unique_ptr<OutputStream> m_cout;
    unique_ptr<ServerSocket> m_ServerSocket;
    string serverName{};
    string portNumber{};

    mutex m_mutex;
public:
    StartServer(const string& serverName, const string& portNumber);
    void sendBroadcastTextMessage();
    void readFromClient(int clientSocket);
    void startReadFromClientThread(int clientSocket);
    void listenClientConnections();
    int Run();
};
