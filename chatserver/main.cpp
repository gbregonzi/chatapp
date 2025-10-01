#include <iostream>
#include <thread>
#include "chatserver.h"
#include "startserver.h"
#include "../utils/outputStream.h"

constexpr int PORT = 8080;

int main (int argc, const char* argv[]){
    
    string serverName = "localhost";
    string portNumber{to_string(PORT)};
    if (argc > 1 && (string(argv[1]) == "-h" || string(argv[1]) == "--help")) {
        cout << "Usage: " << argv[0] << " [server_name] [port_number]\n";
        cout << "  server_name: The hostname or IP address to bind the server (default: localhost)\n";
        cout << "  port_number: The port number to bind the server (default: " << PORT << ")\n";
        return 0;
    }
    if (argc == 1) {
        cout << "No arguments provided. Using default server name 'localhost' and port " << PORT << ".\n";
    }

    if (argc > 1) {
        serverName = argv[1];
    }
    if (argc > 2) {
        try {
            int portArg = stoi(argv[2]);
            if (portArg > 0 && portArg <= 65535) {
                portNumber = portArg;
            } else {
                cerr << "Invalid port number. Using default port " << PORT << ".\n";
            }
        } catch (const std::exception& e) {
            cerr << "Error parsing port number: " << e.what() << ". Using default port " << PORT << ".\n";
        }
    }
    StartServer startServer(serverName, portNumber);
    startServer.Run();

    return 0;
}
