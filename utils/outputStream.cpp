#include "outputStream.h"

OutputStream::OutputStream(ostream &os) : m_os(os) {};

ostream& OutputStream::operator <<(const string &data) {
		lock_guard<mutex> lock(coutMutex);
        m_os << data;
        return m_os;
}

