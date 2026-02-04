#pragma once
#include <string>
#include <cstring>

#ifdef _WIN32
    typedef int socklen_t;
    #include <windows.h>
    #include <format>
    #define ERROR_CODE WSAGetLastError()
#else
    #define ERROR_CODE errno
#endif

using namespace std;
#include "logger.h"

string getLastErrorDescription();
void logLastError(Logger& logger);
void logLastError(Logger& logger, int errorCode);
void logLastError(Logger& logger, const string & message, int errorCode);
