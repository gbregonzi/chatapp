#include "outputStream.h"

OutputStream::OutputStream(mutex& _mutex, ostream &os) : m_mutex(_mutex), m_os(os) {};

ostream& OutputStream::operator <<(const string &data) {
		lock_guard<mutex> lock(m_mutex);
        m_os << data;
        return m_os;
}

