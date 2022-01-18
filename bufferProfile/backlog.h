/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   backlog.h
 * Author: root
 *
 * Created on March 8, 2017, 3:21 PM
 */

#ifndef CBACKLOG_H
#define CBACKLOG_H

#include <string>
#include <iostream>

using namespace std;

#define NUM_ENTRIES 5
class CBacklog {   
public:
    CBacklog() ;
//    CBacklog(const CBacklog& orig);
    virtual ~CBacklog();
    void addEntry(
        string time, 
        string  rxMbpsStr,
        string numStreams, 
        string pktsStr, 
        string pktsRateStr,
        string dropStr, 
        string hbFillStr,
        string obFillStr,
        string utilString) { 
        
        m_processed[m_currPos] = false;
        m_time[m_currPos] = time; 
        m_rxMbpsStr[m_currPos] = rxMbpsStr;

        m_numStreams[m_currPos] = numStreams;
        m_pktsStr[m_currPos] = pktsStr;
        m_pktsRateStr[m_currPos] = pktsRateStr;
        m_dropStr[m_currPos] = dropStr;
        m_hbFillStr[m_currPos] = hbFillStr;
        m_obFillStr[m_currPos] = obFillStr;
        m_utilString[m_currPos] = utilString; 
        
        m_currPos = (m_currPos + 1) % NUM_ENTRIES;
    }
    void output(bool endDelimiter);
    bool initialize(string filename, string time) ;
    void shutdown(string timeStr);
    void write(string str);
private:
    bool m_processed[NUM_ENTRIES];
    string m_time[NUM_ENTRIES];
    string m_rxMbpsStr[NUM_ENTRIES];
    
    string m_numStreams[NUM_ENTRIES];
    string m_pktsStr[NUM_ENTRIES];
    string m_pktsRateStr[NUM_ENTRIES];
    string m_dropStr[NUM_ENTRIES];
    string m_hbFillStr[NUM_ENTRIES];
    string m_obFillStr[NUM_ENTRIES];
    string m_utilString[NUM_ENTRIES];
    
    unsigned int m_currPos;
    
    FILE *m_file;


};

#endif /* CBACKLOG_H */

