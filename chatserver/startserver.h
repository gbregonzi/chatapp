#pragma once
#include <string>
#include <thread>
#include <mutex>
#include "chatserver.h"
#include "../utils/logger.h"

using namespace std;

class StartServer {
private:
    jthread readFromClientThread;
    jthread serverBroadcastThread;
    Logger& m_Logger;
    unique_ptr<ChatServer> m_chatServer;
public:
    StartServer(const string& logFileName, const string& serverName, const string& portNumber);
    int Run();
};
