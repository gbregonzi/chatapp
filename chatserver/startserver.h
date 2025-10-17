#pragma once
#include <string>
#include <thread>
#include <mutex>
#include "chatserver.h"
#include "../utils/outputStream.h"
#include "../utils/logger.h"

using namespace std;

class StartServer {
private:
    jthread readFromClientThread;
    jthread serverBroadcastThread;
    Logger& m_Logger;
    ServerSocket& m_ServerSocket;
    string serverName{};
    string portNumber{};
public:
    StartServer(const string& serverName, const string& portNumber);
    int Run();
};
