#pragma once
#include <filesystem>
#include <fstream>
#include <queue>
#include <mutex>
#include <type_traits>
#include <atomic>
#include <thread>
#include <fmt/core.h>

using namespace std;
using namespace std::filesystem;

enum class LogLevel { Debug, Info, Warning, Error };
constexpr size_t MAX_LOG_FILE_SIZE = 10000000; // 10 MB

class Logger {
private:
	ofstream os{};
	path m_path{};
	mutex m_mutex;
	int m_MaxLogSize;
	atomic<bool> doneFlag{ false };
	queue<string> m_queue{};
	jthread m_ThreadProcessMessages;
	
	//writeLog - Write a log message to the log file
	// message - The message to be logged 
	// Returns true if the message was successfully written to the log file
	bool writeLog(const string& message) noexcept;
	
	// openFile - Opens the log file for writing
	// Returns true if the file was successfully opened
	bool openFile();
	
	// closeFile - Closes the log file
	// Returns true if the file was successfully closed
	bool closeFile();
	
	// renameLogFile - Renames the log file if it exceeds the maximum size
	// Returns true if the log file was successfully renamed
	bool renameLogFile();
	
	// tostring - Converts the log level to a string representation
	// level - Convert the log level to a string representation
	// Returns the string representation of the log level
	string toString(const LogLevel& level);
	
	// logError - Logs an error message with an error code
	// errorMsg - The error message to be logged
	// errorCode - The error code to be logged
	void logError(const string_view errorMsg, int errorCode);

public:
	Logger() = delete; // Disable default constructor
	Logger(const Logger&) = delete; // Disable copy constructor
	Logger& operator=(const Logger&) = delete; // Disable copy assignment operator
	Logger& operator=(const Logger&&) = delete; // Disable move assignment operator

	// Constructor
	// fileName - Initializes the logger with a file name
	// maxLogSize - The maximum size of the log file before it is renamed
	Logger(const string& fileName, size_t maxLogSize);
	
	// Destructor - Cleans up the logger
	~Logger();

	// log - Logs a message with a specific log level
	// logLevel - The log level of the message
	// message - The message to be logged
	void log(const LogLevel& logLevel, const string& message);
	
	// log - Logs a formatted message with a specific log level
	// logLevel - The log level of the message
	// fmt - The format string
	// args - The arguments to be formatted into the string
    template<typename ...Args>
    void log(const LogLevel& logLevel, const string& _fmt, Args&& ...args){
		string formatted = fmt::vformat(_fmt, fmt::make_format_args(forward<Args>(args)...));
		log(logLevel, formatted);
	}

	// processingMessages - Starts a thread to process log messages
	void processingMessages();

	// logSize - Returns the size of the log file
	size_t logSize();

	// isOpen - Checks if the log file is open
	bool isOpen();

	// eof - Checks if the end of the file has been reached
	bool eof();

	// isDone - Checks if the logger has finished processing messages
	bool isDone();
	
	// setDone - Sets the done flag to indicate if the logger has finished processing messages
	// done - The done flag to be set
	void setDone(bool done);
	
	// stopProcessing - Stops the logger from processing messages
	void stopProcessing();
};

// LoggerFactory - Singleton factory for Logger instances	
struct LoggerFactory {
	static Logger& getInstance(size_t maxLogSize = MAX_LOG_FILE_SIZE) {
		static Logger instance(string("chatServer.log"), maxLogSize); // 1 MB max size
		return instance;
	}
};
