/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   NTUtils.cpp
 * Author: root
 * 
 * Created on May 19, 2016, 1:23 PM
 */

#include <stdarg.h>
#include <syslog.h>

#include <iostream>
#include <fstream>
#include <string>


#include "NTUtils.h"

#define DAEMON_NAME "ntfilterd"


NtConfigStream_t    NTUtils::s_hCfgStream;
string              NTUtils::s_streamID;
ofstream            NTUtils::s_logFile;

NTUtils::NTUtils() {
}

//NTUtils::NTUtils(const NTUtils& orig) {
//}

NTUtils::~NTUtils() {
}

bool NTUtils::init(string streamid) {
    s_streamID = streamid;

    int status;

    // Initialize the NTAPI library and thereby check if NTAPI_VERSION can be used together with this library
    if ((status = NT_Init(NTAPI_VERSION)) != NT_SUCCESS) {
        char *errorBuffer = (char *)malloc(NT_ERRBUF_SIZE); // Error buffer
        // Get the status code as text
        NT_ExplainError(status, errorBuffer, sizeof (errorBuffer));
        fprintf(stderr, "NT_Init() failed: %s\n", errorBuffer);
        free(errorBuffer);
        return false;
    }

    // Open a config stream to assign a filter to a stream ID.
    if ((status = NT_ConfigOpen(&s_hCfgStream, "TestStream")) != NT_SUCCESS) {
        char *errorBuffer = (char *)malloc(NT_ERRBUF_SIZE); // Error buffer
        // Get the status code as text
        NT_ExplainError(status, errorBuffer, sizeof (errorBuffer));
        fprintf(stderr, "NT_ConfigOpen() failed: %s\n", errorBuffer);
        free(errorBuffer);
        return false;
    }
    return true;
}


bool NTUtils::openLog(string logFile) {
    
    //Set our Logging Mask and open the Log
    setlogmask(LOG_UPTO(LOG_NOTICE));
    openlog(DAEMON_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER);

    
    if (logFile.length() > 0) {
        s_logFile.open(logFile.c_str(), ios::app);   
    }
    
    return s_logFile.is_open();
}

void NTUtils::closeLog() {
    s_logFile.close();    
}

void NTUtils::log(int level, const char *fmtStr, ...) {
    va_list args;
    va_start(args, fmtStr);

    char *str1, *str2;
    int byteCnt = vasprintf(&str1, fmtStr, args);
    asprintf(&str2, "%s\n", str1);

    va_end(args);
   
    if (s_logFile.is_open()) {
        s_logFile.write(str2, byteCnt+1);
        s_logFile.flush();
    }

    if (level <= LOG_NOTICE) {
        syslog(level, str1);
    }
    
    free(str1);    
    free(str2);
}

uint32_t NTUtils::NTPL(const char *fmtStr, ...) {
    uint32_t status = 0;
    va_list args;
    va_start(args, fmtStr);

    char *ntplCmd;
    vasprintf(&ntplCmd, fmtStr, args);
    va_end(args);

    NtNtplInfo_t ntplInfo;


    if ((status = NT_NTPL(s_hCfgStream, ntplCmd, &ntplInfo, NT_NTPL_PARSER_VALIDATE_NORMAL)) == NT_SUCCESS) 
    {
        status = ntplInfo.ntplId;
        NTUtils::log(LOG_INFO, "    \"%s\" -> %4d", ntplCmd, status);
    } else {
        char *errorBuffer = (char *)malloc(1024);
        NTUtils::log(LOG_ERR, "    \"%s\" -> %4d", ntplCmd, status);
        NT_ExplainError(status, errorBuffer, sizeof (errorBuffer) - 1);
        NTUtils::log(LOG_ERR, "     NT_NTPL() failed: %s", errorBuffer);
        NTUtils::log(LOG_ERR, "         NTPL errorcode: %X", ntplInfo.u.errorData.errCode);
        NTUtils::log(LOG_ERR, "         %s", ntplInfo.u.errorData.errBuffer[0]);
        NTUtils::log(LOG_ERR, "         %s", ntplInfo.u.errorData.errBuffer[1]);
        NTUtils::log(LOG_ERR, "         %s", ntplInfo.u.errorData.errBuffer[2]);
        free(errorBuffer);
        status = 0;
    }

    free(ntplCmd);
    return status;
}

