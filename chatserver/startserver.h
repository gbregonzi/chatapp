#pragma once
#include <string>
#include <thread>
#include <mutex>
#include "chatServer.h"
#include "../utils/logger.h"

using namespace std;

class StartServer {
private:
    jthread readFromClientThread;
    jthread serverBroadcastThread;
    Logger& m_Logger;
    unique_ptr<chatServer> m_chatServer;
    string serverName{};
    string portNumber{};
public:
    StartServer(const string& logFileName, const string& serverName, const string& portNumber);
    int Run();
};
