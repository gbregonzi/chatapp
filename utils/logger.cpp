#include <string>	
#include <thread>
#include <fstream>
#include <iostream>
#include <sstream>
#include <system_error>
#include <chrono>
#include <format>

#include "logger.h"


Logger::Logger(const string_view fileName, int _maxLogSize) : maxLogSize(_maxLogSize)
{
	static_assert(is_constructible_v<Logger, string, int>, "Logger must be constructible with a string.");
	static_assert(!is_copy_constructible_v<Logger>, "Logger should not be copy constructible");
	static_assert(!is_copy_assignable_v<Logger>, "Logger should not be copy assignable");
	static_assert(!is_move_assignable_v<Logger>, "Logger should not be move assignable");
	cout << "--- Logger Constructor Called! ---\n";
	m_path = current_path().string() + R"(\Log\)" + fileName.data();
	renameLogFile();
	if (openFile()) {
		processingMessages(); // Start the processing thread
	}
	else {
		cout << "Failed to open log file: " << m_path.filename().string() << "\n";
	}
}

Logger::~Logger() {
	//cout << "Logger destructor called" << "\n";
	while(!m_queue.empty()) {
		this_thread::sleep_for(chrono::milliseconds(100)); // Wait for the queue to be processed
	}	
	stopProcessing(); // Ensure we stop processing when the logger is destroyed
}

bool Logger::closeFile() {
	if (os.is_open()) {
		os.flush();
		os.close();
		return true;
	}
	return false;
}

size_t Logger::size() {
	if (isOpen()) {
		return filesystem::file_size(m_path);
	}
	else {
		return 0; 
	}

}

bool Logger::isOpen() {
	return os.is_open();
}

bool Logger::eof() {
	return os.eof();
}

bool Logger::renameLogFile() {
	cout << "Renaming Log file..." << "\n";
	if (isOpen()) {
		closeFile(); // Close the file before renaming
	}
	if(!exists(m_path)) {
		cout << "There is no log file to rename:" << m_path.string() << "\n";
		return true;
	}
	auto now = chrono::system_clock::now();
	time_t time = chrono::system_clock::to_time_t(now);
	auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) % 1000;
	tm tm_buf;
	if (localtime_s(&tm_buf, &time)) {
		logError(string("localtime_s failed in renameLogFile for:" + m_path.string()), errno);
		return false;
	}

	char timeStr[30]{ 0 };
	strftime(timeStr, sizeof(timeStr), "%Y%m%d%H%M%S", &tm_buf);
	ostringstream os;
	os << timeStr << '.' << setfill('0') << setw(3) << ms.count();
	string extension = m_path.extension().string();
	m_path.replace_extension(""); // Remove the extension for renaming
	string newFileName = m_path.string() + "_" + os.str() + extension;
	
	if (exists(newFileName)) {
		remove(newFileName);
	}
	m_path += extension; // Restore the original path with extension
	rename(m_path, newFileName);
	if (!exists(newFileName)) {
		cout << "Failed to rename log file to: " << newFileName << "\n";
	}
	else {
		cout << "Log file renamed to: " << newFileName << "\n";
	}
	return true;
}

bool Logger::isDone() {
	return doneFlag;
}
void Logger::stopProcessing() {
	cout << "Stopping Logger processing messages" << "\n";	
	doneFlag = true;
	if (os.is_open()) {
		os.flush();
		os.close();
	}
}

void Logger::log(const LogLevel &logLevel, const string& message) {
    lock_guard<mutex> lock(m_mutex);
    auto now = chrono::system_clock::now();
    auto time = chrono::system_clock::to_time_t(now);
    auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) % 1000;
    tm tm_buf;
    if (localtime_s(&tm_buf, &time) != 0) {
        logError(string("localtime_s failed in writeLog for:" + m_path.filename().string()), errno);
    }

    ostringstream oss;
    oss << "[" << put_time(&tm_buf, "%F %T") << ':' << setfill('0') << setw(3) << ms.count() << "] "
        << "[" << toString(logLevel) << "] " << message;
	
    if (!doneFlag) {
        m_queue.emplace(oss.str());
    }
    else {
        cout << "Logger is stopped, cannot log message: " << message << "\n";
    }
}

void Logger::logError(const string_view errorMsg, int errorCode) {
	error_code ec(errorCode, system_category());
	wcerr << L"Error: " << errorMsg.data() << L"\nError Code(" << errorCode << L"): " << ec.message().c_str() << "\n";
}

bool Logger::openFile() {
	try {
		path workDir = m_path.parent_path();
		if (!is_directory(workDir)) {
			cout << "Creating path:" << workDir.string() << "\n";
			filesystem::create_directories(workDir);
		}
		if (!filesystem::exists(workDir)) {
			cout << "Could not find path:" << workDir.string() << "\n";
			return false;
		}
		os.open(m_path, ios::app | ios::binary);

		if (!os.is_open()) {
			cout << "Unable to open file:" << m_path.string() << "\n";
			logError(string("Unable to open file:" + m_path.string()), errno);

			return false;
		}
	}
	catch (const exception& ex) {
		cout << ex.what() << "\n";
		return false;
	}
	return true;
}

bool Logger::writeLog(const string& message) noexcept
{

	os << message << "\n";

    if (!os.good()) {
        logError(string("Could not write to log file:" + m_path.filename().string()), errno);
        return false;
    }
	return true;
}

string Logger::toString(const LogLevel &level) {
	switch (level) {
		case LogLevel::Debug: return "Debug";
		case LogLevel::Info: return "Info";
		case LogLevel::Warning: return "Warning";
		case LogLevel::Error: return "Error";
		default: return "Unknown";
	}
}

void Logger::processingMessages() {
	cout << "Starting Logger processing messages" << "\n";
	thread processingMessagesThread([this] {
		while (!doneFlag) {
			if (size() > maxLogSize) {
				renameLogFile(); // Rename the log file if it exceeds the size limit
				if (!openFile()) { // Reopen the log file after renaming
					logError("Failed to reopen log file after renaming", errno);
					cout << "loggger stop processing messages" << "\n";
					return; // Exit if we cannot reopen the file
				}
			}
			unique_lock<mutex> lc(m_mutex);
			if (!m_queue.empty()) {
				if (writeLog(m_queue.front().c_str())) {
					m_queue.pop();
				}
				else {
					logError("Failed to write log message", errno);
				}
				lc.unlock();
			}
			else {
				lc.unlock();
				this_thread::sleep_for(chrono::milliseconds(100)); // Sleep to avoid busy waiting
			}
		}
	});
	processingMessagesThread.detach(); // Detach the thread to run independently
}
