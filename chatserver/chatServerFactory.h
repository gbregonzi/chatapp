#pragma once
#include <memory>
#include <string>

#include "chatServer.h"
#include "handleConnectionsWindows.h"
//#include "handleConnectionsLinux.h"
using namespace std;
struct chatServerFactory {
    inline static unique_ptr<chatServer> getInstance(Logger &logger, 
                                                        const string& serverName, 
                                                        const string& portNumber) {
#ifdef _WIN32
        return make_unique<HandleConnectionsWindows>(logger, serverName, portNumber);
#else
        return make_unique<HandleConnectionsLinux>(logger, serverName, portNumber);
#endif
    }
};