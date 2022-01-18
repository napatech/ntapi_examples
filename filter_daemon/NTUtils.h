/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   NTUtils.h
 * Author: root
 *
 * Created on May 19, 2016, 1:23 PM
 */

#ifndef NTUTILS_H
#define NTUTILS_H

#include <string>
#include <syslog.h>

#include <nt.h>
using namespace std;

class NTUtils {
public:
    NTUtils();
    NTUtils(const NTUtils& orig);
    virtual ~NTUtils();
    
    
    static bool init(string streamID);
    static bool openLog(string logFile = "");
    static void closeLog();
    static void log(int level, const char *fmtStr, ...);
    static uint32_t NTPL(const char *fmtStr, ...);
    
private:
    static string s_streamID;
    static NtConfigStream_t s_hCfgStream;
    static ofstream s_logFile;
};

#endif /* NTUTILS_H */

