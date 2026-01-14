#include "util.h"
// getLastErrorDescription - retrieves a description for the last error code
string getLastErrorDescription() {
    string result;
    int errorCode = ERROR_CODE;
#ifdef _WIN32
    char *errorMsg = NULL;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&errorMsg,
        0,
        NULL
    );
    if (!errorMsg) {
        return format("WSA Error {}: <unknown>", errorCode);;
    }
    result = format("{}: {}", ERROR_CODE, errorMsg);
    LocalFree(errorMsg);
#else
    result = strerror(ERROR_CODE);
#endif
    return result;
}

void logLastError(Logger& logger)
{
    int errorCode{0};
#ifdef _WIN32
    errorCode = WSAGetLastError();
#else
    errorCode = errno;
#endif
    logger.log(LogLevel::Error, "{}:Error code:{}",__func__, errorCode);
    logger.log(LogLevel::Error, "{}:Error description:{}",__func__, getLastErrorDescription());
}

void logLastError(Logger& logger, int errorCode)
{
    logger.log(LogLevel::Error, "{}:Error code:{}",__func__, errorCode);
    logger.log(LogLevel::Error, "{}:Error description:{}",__func__, getLastErrorDescription());
}

void logLastError(Logger& logger, const string & message, int errorCode)
{
    logger.log(LogLevel::Error, "{}:{}", __func__, message);
    logger.log(LogLevel::Error, "{}:Error code:{}",__func__, errorCode);
    logger.log(LogLevel::Error, "{}:Error description:{}",__func__, getLastErrorDescription());
}
