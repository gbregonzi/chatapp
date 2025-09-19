#pragma once

#include <iostream>
#include <mutex>
#include <ostream>
using namespace std;

class OutputStream  {
private:
    ostream &m_os;
    mutex& m_mutex;    
public:
    OutputStream(mutex& _mutex, ostream &os);
    ostream& operator <<(const string &data);
};


