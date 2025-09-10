#include <iostream>
#include "chatserver.h"

int main (int argc, const char* argv[]){
    Socket serverSocket;
    if (serverSocket.getIsConnected()) {
        serverSocket.listenClientConnections();
    } else {
        cerr << "Server failed to connect." << endl;
        return EXIT_FAILURE;
    }

    return 0;
}
