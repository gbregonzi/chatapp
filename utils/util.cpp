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
