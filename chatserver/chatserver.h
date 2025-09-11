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

class ServerSocket{
    private:
        int m_sockfd;
        atomic<bool> m_IsConnected{false};
        struct sockaddr_in m_ServerAddr;
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
        //void readFromClient(int sd, atomic<bool> &chatActive);

        // readFromClientThread - thread function to read messages from the connected client
        // sd: the socket descriptor of the connected client
        // chatActive: atomic boolean to control the chat session
        //void readFromClientThread(int sd, atomic<bool> &chatActive);

        // threadBradcastMessage - processes messages from the read message queue
        void threadBradcastMessage();

        // serverSendBroadcastMessage - sends messages from the send message queue to clients
        //void serverSendBroadcastMessage();

        // getHostNameIP - retrieves and prints the server's hostname and IP address
        void getHostNameIP();
        
    public:
        // Constructor
        ServerSocket();
        
        // getError - prints error code and description
        // errorCode: the error code to be printed
        void getError(int errorCode);

        // getClientIP - retrieves and prints the connected client's IP address and port
        // sd: the socket descriptor of the connected client
        // Returns true if successful, false otherwise
        bool getClientIP(int sd);

        // getIsConnected - returns the connection status
        bool getIsConnected() const;

        // setIsConnected - sets the connection status
        void setIsConnected(bool isConnected);

        // addBroadcastMessage - adds a message to the broadcast queue
        // message: the message to be added
        // Returns 0 on success, -1 on failure
        int addBroadcastTextMessage(string message);
        
        // Listen - listens for incoming client connections
        void listenClientConnections();
        
        // readMessage - reads a message from a specific client
        // message: output parameter to hold the received message
        // Returns number of bytes read, or -1 on error/disconnection
        ssize_t readMessage(string &message);
        
        // getClientCount - returns the number of currently connected clients
        ssize_t getClientCount();

        // closeSocket - closes the socket 
        // sd: the socket descriptor 
        void closeSocket(int sd);

        // Destructor
        ~ServerSocket();

};

