#pragma once

#include <iostream>
#include <mutex>
#include <ostream>
using namespace std;

class OutputStream  {
private:
    ostream& m_os;
    mutex& m_mutex;    
public:

    OutputStream(mutex& _mutex, ostream& os) : m_mutex(_mutex), m_os(os) {}
    template<typename T>
    OutputStream& operator <<(const T& data) {
        lock_guard<mutex> lock(m_mutex);
        m_os << data;
        return *this;
    }
    template<typename T>
    void log(const T& data) {
        lock_guard<mutex> lock(m_mutex);
        m_os << data << "\n";
    }
};


