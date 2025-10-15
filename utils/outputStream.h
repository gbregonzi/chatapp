#pragma once

#include <iostream>
#include <mutex>
#include <ostream>
using namespace std;

class OutputStream  {
private:
    ostream& m_os;
    mutex m_Mutex;    
public:

    //OutputStream(mutex& _mutex, ostream& os) : m_Mutex(_mutex), m_os(os) {}
    OutputStream(ostream& os) : m_os(os) {}
    
    template<typename T>
    OutputStream& operator <<(const T& data) {
        lock_guard<mutex> lock(m_Mutex);
        m_os << data;
        return *this;
    }
    
    template<typename T>
    void log(const T& data) {
        lock_guard<mutex> lock(m_Mutex);
        m_os << data << "\n";
    }
};

struct LogFactory{
    static unique_ptr<OutputStream>create(ostream& os = cout){
            return make_unique<OutputStream>(os);
    } 
};


