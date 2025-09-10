#include <iostream>
#include <string>
#include "clientSocket.h"

int main (int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <server:ip> <server_port>" << std::endl;
        return 1;
    }

    string server_ip = argv[1];
    int server_port = stoi(argv[2]);
    cout << "Server IP: " << server_ip << ", Server Port: " << server_port << endl; 
    ClientSocket client(server_ip, server_port);
    if (client.connect() == 0) {
        cout << "Connected to server successfully!" << endl;
        client.readMessageThread();
        client.sendMessageThread();
    } else {
        cout << "Failed to connect to server." << endl;
    }

    return 0;
}