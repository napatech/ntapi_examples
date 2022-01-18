/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   NTFileHandler.h
 * Author: root
 *
 * Created on May 12, 2016, 1:56 PM
 */

#ifndef NTFILEHANDLER_H
#define NTFILEHANDLER_H

#include <iostream>
#include <fstream>
#include <string>
#include <set>

#include "IPAddress.h"

using namespace std;


class NTFileHandler {
public:
    NTFileHandler();
    NTFileHandler(const NTFileHandler& orig);
    virtual ~NTFileHandler();
    bool process(void);
    bool open(string filterFile);
    void close(void);
    bool watchFile(bool *applicationRunning);
    bool readFile();

private:
    string m_filterFile;
    ofstream m_ntfilter;    
    struct tm m_prevTime;
    
    set<IPAddress> m_appliedAddrs;
    set<IPAddress> m_readAddrs;
};

#endif /* NTFILEHANDLER_H */

