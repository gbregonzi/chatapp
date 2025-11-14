#pragma once
#include <string>

#ifdef _WIN32
typedef int socklen_t;
    #include <windows.h>
    #include <format>
    #define ERROR_CODE WSAGetLastError()
#else
    #define ERROR_CODE errno
#endif

using namespace std;

string getLastErrorDescription();
