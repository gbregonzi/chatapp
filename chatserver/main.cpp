#include <iostream>
#include <thread>
#include "chatserver.h"
#include "startserver.h"
#include "../utils/outputStream.h"

constexpr int PORT = 8080;

void usage(const char* _argv, const string& serverName, const string& logFileName){
    cout << "Usage: " << _argv << " [server_name] [port_number] [log_file_name]\n";
    cout << "  server_name: The hostname or IP address to bind the server (default: " << serverName << ")\n";
    cout << "  port_number: The port number to bind the server (default: " << PORT << ")\n";
    cout << "  log_file_name: The name of the log file to be create (default: " << logFileName << ")\n";
}

int main (int argc, const char* argv[]){
    
    string serverName = "localhost";
    string portNumber{to_string(PORT)};
    string logFileName{"chatServer.log"};

    if (argc > 1 && (string(argv[1]) == "-h" || string(argv[1]) == "--help")) {
        usage(argv[0], serverName, logFileName);
        return 0;
    }
    if (argc == 1) {
        cout << "No arguments provided. Using default server name: " << serverName << " port: " << PORT << " and Log file name: " << logFileName << "\n";
    }

    if (argc > 1) {
        try{
            int arg = stoi(argv[1]);
            if (arg) {
                cout << "Invalid argument\n";
                usage(argv[0], serverName, logFileName);
                return 0;
            }
        } catch (const std::exception& e) {
            // Not an integer, assume it's a valid server name
            serverName = argv[1];
        }
    }
    if (argc > 2) {
        try {
            int portArg = stoi(argv[2]);
            if (portArg > 0 && portArg <= 65535) {
                portNumber = argv[2];
            } else {
                cout << "Invalid port number.\nUsing default port: " << portNumber << ".\n default log file name: " << logFileName << "\n";
            }
        } catch (const std::exception& e) {
            cout << "Error parsing port number: " << e.what() << ".\nUsing default port: " << portNumber << ", default log file name: " << logFileName << "\n";
        }
    }
    if (argc > 3){
        logFileName = string(argv[3]);
    }
    else {
        cout << "Log file name not provided, Using default log file name: " << logFileName << "\n";
    }
    StartServer startServer(logFileName, serverName, portNumber);
    startServer.Run();

    return 0;
}
