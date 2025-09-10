#include <string_view>
#include <atomic>
#include <queue>
#include <unordered_set>
#include <mutex>
#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <netinet/in.h>
#endif

using namespace std;

constexpr unsigned short PORT{8080};
constexpr int MAX_QUEUE_CONNECTINON{10};
constexpr int BUFFER_SIZE{1024};

class Socket{
    private:
        int serverSocket;
        atomic<bool> isConnected{false};
        struct sockaddr_in serverAddr;
        mutex m_mutex;

        queue<pair<int, string>> broacastMessageQueue;
        queue<string> sendMessageQueue;
        unordered_set<int> clientSockets;

        // writeToClient - sends a specific message to the connected client
        // Retunrs true if successful, false otherwise
        // sd: the socket descriptor of the connected client
        // message: the message to be sent
        bool writeToClient(int sd, const string_view message);

        // readFromClient - reads messages from the connected client
        // sd: the socket descriptor of the connected client
        // chatActive: atomic boolean to control the chat session
        void readFromClient(int sd, atomic<bool> &chatActive);

        // readFromClientThread - handles reading messages in a chat session
        // sd: the socket descriptor of the connected client
        // chatActive: atomic boolean to control the chat session
        void readFromClientThread(int sd, atomic<bool> &chatActive);

        // threadBradcastMessage - processes messages from the read message queue
        void threadBradcastMessage();

        // brodcastMessage - sends a message to all connected clients
        //void brodcastMessage();

        // serverSendBroadcastMessage - sends messages from the send message queue to clients
        void serverSendBroadcastMessage();

        // getError - prints error code and description
        // errorCode: the error code to be printed
        void getError(int errorCode);
        // getHostNameIP - retrieves and prints the server's hostname and IP address
        void getHostNameIP();
        
        // getClientIP - retrieves and prints the connected client's IP address and port
        // sd: the socket descriptor of the connected client
        // Returns true if successful, false otherwise
        bool getClientIP(int sd);
    public:
        // Constructor
        Socket();
        

        // getIsConnected - returns the connection status
        bool getIsConnected() const;

        
        // Listen - listens for incoming client connections
        void listenClientConnections();
        
        // Destructor
        ~Socket();

};

