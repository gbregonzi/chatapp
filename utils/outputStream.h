#pragma once

#include <iostream>
#include <mutex>
#include <ostream>
using namespace std;

class OutputStream  {
private:
    ostream &m_os;
    mutex coutMutex;    
public:
    OutputStream(ostream &os);
    ostream& operator <<(const string &data);
};


