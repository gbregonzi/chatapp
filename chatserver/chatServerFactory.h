#pragma once
#include <memory>
#include <string>

#include "chatserver.h"
#ifdef _WIN32
        #include "handleConnectionsWindows.h"
#else
        #include "handleConnectionsLinux.h"
#endif
using namespace std;
struct chatServerFactory {
    inline static unique_ptr<ChatServer> getInstance(Logger &logger, 
                                                        const string& serverName, 
                                                        const string& portNumber) {
#ifdef _WIN32
        return make_unique<HandleConnectionsWindows>(logger, serverName, portNumber);
#else
        return make_unique<HandleConnectionsLinux>(logger, serverName, portNumber);
#endif
    }
};