#include <string>	
#include <thread>
#include <fstream>
#include <iostream>
#include <sstream>
#include <system_error>
#include <chrono>
#include <format>

#include "logger.h"


Logger::Logger(const string& fileName, size_t maxLogSize) : m_MaxLogSize(maxLogSize)
{
	static_assert(is_constructible_v<Logger, string, int>, "Logger must be constructible with a string.");
	static_assert(!is_copy_constructible_v<Logger>, "Logger should not be copy constructible");
	static_assert(!is_copy_assignable_v<Logger>, "Logger should not be copy assignable");
	static_assert(!is_move_assignable_v<Logger>, "Logger should not be move assignable");
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
	stopProcessing(); // Ensure we stop processing when the logger is destroyed
}

bool Logger::closeFile() {
	lock_guard<mutex> Lock(m_mutex);
	if (os.is_open()) {
		os.flush();
		os.close();
		return true;
	}
	return false;
}

size_t Logger::logSize() {
	if (isOpen()) {
		return filesystem::file_size(m_path);
	}
	else {
		return 0; 
	}

}

bool Logger::isOpen() {
	lock_guard<mutex> Lock(m_mutex);
	return os.is_open();
}

bool Logger::eof() {
	lock_guard<mutex> Lock(m_mutex);
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
	ostringstream oss;
	oss << timeStr << '.' << setfill('0') << setw(3) << ms.count();
	string extension = m_path.extension().string();
	m_path.replace_extension(""); // Remove the extension for renaming
	string newFileName = m_path.string() + "_" + oss.str() + extension;
	lock_guard<mutex> Lock(m_mutex);
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
	return doneFlag.load();
}

void Logger::setDone(bool done) {
	if (done){
		m_ThreadProcessMessages.request_stop();
	}
	doneFlag.store(done);
}

void Logger::stopProcessing() {
	cout << "Stopping Logger processing messages" << "\n";	
	while(!doneFlag.load())
	{
		{
			lock_guard<mutex> Lock(m_mutex);
			if (m_queue.empty()) {
				m_ThreadProcessMessages.request_stop();
				doneFlag.store(true);
				if (os.is_open()) {
					os.flush();
					os.close();
				}
				break;
			}
		}
		this_thread::sleep_for(chrono::milliseconds(100)); // Wait for the queue to be processed
	}
	
}

void Logger::log(const LogLevel &logLevel, const string& message) {
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
	
    if (!doneFlag.load()) {
		lock_guard<mutex> lock(m_mutex);
        m_queue.emplace(oss.str());
    }
    else {
        cout << "Logger is stopped, cannot log message: " << oss.str() << "\n";
    }
}

void Logger::logError(const string_view errorMsg, int errorCode) {
	error_code ec(errorCode, system_category());
	wcerr << L"Error: " << errorMsg.data() << L"\nError Code(" << errorCode << L"): " << ec.message().c_str() << "\n";
}

bool Logger::openFile() {
	try {
		lock_guard<mutex> Lock(m_mutex);
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
	os.flush();
	if (!os.good()) {
		logError(string("Could not flush log file:" + m_path.filename().string()), errno);
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
	 m_ThreadProcessMessages = jthread([this](stop_token sToken) {
		while (!sToken.stop_requested()) {
			if (logSize() > m_MaxLogSize) {
				renameLogFile(); // Rename the log file if it exceeds the size limit
				if (!openFile()) { // Reopen the log file after renaming
					logError("Failed to reopen log file after renaming", errno);
					cout << "loggger stop processing messages" << "\n";
					return; // Exit if we cannot reopen the file
				}
			}
			unique_lock<mutex> lock(m_mutex);
			if (!m_queue.empty()) {
				if (writeLog(m_queue.front().c_str())) {
					m_queue.pop();
				}
				else {
					logError("Failed to write log message", errno);
				}
				lock.unlock();
			}
			else {
				lock.unlock();
				this_thread::sleep_for(chrono::milliseconds(100)); // Sleep to avoid busy waiting
			}
		}
	});
}
