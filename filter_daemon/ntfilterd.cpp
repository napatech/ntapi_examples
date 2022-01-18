/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   ntfilterd.cpp
 * Author: root
 *
 * Created on May 16, 2016, 11:10 AM
 */
#include <cstdlib>

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <string>
#include <dirent.h>


#include <errno.h>
#include <sys/types.h>

#include <nt.h>
#include "ntfilterd.h"
#include "NTFileHandler.h"
#include "NTUtils.h"



#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))


using namespace std;


#define OPTION_HELP             (1<<1)

#define OPTION_SHORT_HELP       'h'

#define OPTION_DEBUG            'd'
static bool opt_debug = 0;

#define OPTION_KILL             'k'

#define OPTION_VERBOSE          'v'
static bool opt_verbose = false;

#define OPTION_FILE             'f'
static bool opt_filename = false;

#define OPTION_LOGFILE          'l'
static bool opt_logFilename = false;

#define OPTION_STREAMID         's'
static bool opt_streamid = false;

#define OPTION_COLOR            'c'
static uint opt_color = 0;

#define OPTION_PRIORITY            'p'
static uint opt_priority = 0;

static struct option long_options[] = {
    {"help", no_argument, 0, OPTION_HELP},
    {0, 0, 0, 0}
};

NTFileHandler fHandler;


static bool applicationRunning = true;

void StopDaemon(int signo) {
    NTUtils::log(LOG_NOTICE, "terminating  ntfilterd - signo: %d\n", signo);
    applicationRunning = false;
    //fHandler.killWatch();
}

static void _Usage(void) {
    fprintf(stderr, "%s\n", usageText);
    exit(1);
}

void process(NTFileHandler &myfHandler) {
    NTUtils::log(LOG_NOTICE, "Writing to my Syslog from main");
    myfHandler.process();
}




#define PIDFILE "/var/run/ntfilterd.pid"

bool writePIDFile(pid_t pid) {
    FILE* fd = NULL;
    fd = fopen(PIDFILE,"w");
    rewind(fd);
   
    if (NULL != fd) {
        fprintf(fd, "%d", pid);
        fclose(fd);
        return true;
    } else {
        NTUtils::log(LOG_ERR, "Failed to open PIDFile!");
        return false;    
    }
}

pid_t readPIDFile() {
    FILE* fd = NULL;
    pid_t pid = 0;
    fd = fopen(PIDFILE,"r");    
    
    if (NULL != fd) {
        fscanf(fd, "%d", &pid);
    }
    return pid;
}

int main(int argc, char *argv[]) {
    int option;
    char *p = argv[0];
    int option_index = 0;
    string filename;
    string logFilename;
    string streamid;
    uint ntplIdx[16] = {0};
    uint ntplIdxCnt = 0;
       
    signal(SIGINT, StopDaemon);


    //-----    -----    -----    -----    -----    -----    -----    -----
    // Process command line options
    // Strip path
    uint i = strlen(&argv[0][0]);
    for (; i != 0; i--) {
        if ((p[i - 1] == '.') || (p[i - 1] == '\\') || (p[i - 1] == '/')) {
            break;
        }
    }
    int res = 0;

    while ((option = getopt_long(argc, argv, "f:l:s:c:p:dhkv", long_options, &option_index)) != -1) {
        switch (option) {
            case OPTION_HELP:
            case OPTION_SHORT_HELP:
                _Usage();
                break;
            case OPTION_KILL:
                printf("entering kill");
                res = kill(readPIDFile(), SIGUSR1);
                printf("res: %d\n", res);
                if (res != -1) {
                    printf("Running Instance terminated.\n");
                } else {
                    printf("No Running Instances found.  Cleaning up...\n");
                }
                writePIDFile(0);
                exit(0);
                break;
            case OPTION_VERBOSE:
                opt_verbose = true;
                break;
            case OPTION_DEBUG:
                opt_debug = true;
                break;
            case OPTION_FILE:
                filename = optarg;
                opt_filename = true;
                break;
            case OPTION_LOGFILE:
                logFilename = optarg;
                opt_logFilename = true;
                break;
            case OPTION_STREAMID:
                opt_streamid = true;
                streamid = optarg;
                break;
            case OPTION_COLOR:
                opt_color = strtol(optarg, NULL, 10);
                break;
            case OPTION_PRIORITY:
                opt_priority = strtol(optarg, NULL, 10);
                break;
            default:
                printf("Invalid command line option!!!");
                _Usage();
                exit(-1);
                break;
        }
    }
    
    NTUtils::openLog( (logFilename.length() == 0 ? "" : logFilename)  );    

    if (!opt_filename) {
        printf("ERROR: Missing filename.\n");
        NTUtils::log(LOG_ERR, "Missing filename.");
        exit(-1);
    }

    
    // Test if there is an instance running.
    if (readPIDFile()) {
        printf("\nERROR: An instance of this program is already running.\n");
        printf("You must first terminate running instances using the -k option.\n\n");
        exit(-1);
    }


    
    NTUtils::log(LOG_NOTICE, "Napatech Filter Daemon started.");
    NTUtils::log(LOG_NOTICE, "    Filter File: %s", filename.c_str());
    NTUtils::log(LOG_NOTICE, "    Stream ID: %s", streamid.c_str());
    NTUtils::log(LOG_NOTICE, "    Color: %d", opt_color);
    NTUtils::log(LOG_NOTICE, "    Base Priority: %d", opt_priority);

    if (!NTUtils::init(streamid)) {
        NTUtils::log(LOG_ERR, "Napatech system failed to start.");
        exit(-1);
    }

    if (!opt_debug) {
        //-----    -----    -----    -----    -----    -----    -----    -----
        // Spawn the daemon
        //
        pid_t pid, sid;

        //Fork the Parent Process
        pid = fork();

        if (pid < 0) {
            exit(EXIT_FAILURE);
        }

        //We got a good pid, Close the Parent Process
        if (pid > 0) {
            exit(EXIT_SUCCESS);
        }

        //Change File Mask
        umask(0);

        //Create a new Signature Id for our child
        sid = setsid();
        if (sid < 0) {
            exit(EXIT_FAILURE);
        }
        writePIDFile(sid);
        NTUtils::log(LOG_NOTICE, "Napatech filter daemon process ID: %d\n", sid);
        
        sleep(1);
        printf("read %d from pidfile\n", readPIDFile());

        //Change Directory
        //If we cant find the directory we exit with failure.
        if ((chdir("/")) < 0) {
            exit(EXIT_FAILURE);
        }

        //Close Standard File Descriptors
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }
    
    //-----    -----    -----    -----    -----    -----    -----    -----
    //Main Process
    //----------------    

    NTUtils::log(LOG_INFO, "Creating stream assignments...");
 
    // Create a dummy entry to establish the type for the MatchList
    int dummyFilter = NTUtils::NTPL((const char *) "IPMatchList[KeySet=3] = IPv4Addr == [0.0.0.0]");
    NTUtils::NTPL((const char *) "delete=%d", dummyFilter);
    
    dummyFilter = NTUtils::NTPL((const char *) "IPMatchList[KeySet=5] = IPv4Addr == [0.0.0.0]");
    NTUtils::NTPL((const char *) "delete=%d", dummyFilter);
    
    dummyFilter = NTUtils::NTPL((const char *) "IPMatchList[KeySet=4] = IPv6Addr == [0000:0000:0000:0000:0000:0000:0000:0000]");
    NTUtils::NTPL((const char *) "delete=%d", dummyFilter);
    
    dummyFilter = NTUtils::NTPL((const char *) "IPMatchList[KeySet=6] = IPv6Addr == [0000:0000:0000:0000:0000:0000:0000:0000]");
    NTUtils::NTPL((const char *) "delete=%d", dummyFilter);
    
    ntplIdx[ntplIdxCnt++] = NTUtils::NTPL((const char *)"define IPv4_SrcAddr = Field(Layer3Header[12]/32)");
    ntplIdx[ntplIdxCnt++] = NTUtils::NTPL((const char *)"define IPv4_DstAddr = Field(Layer3Header[16]/32)");
    ntplIdx[ntplIdxCnt++] = NTUtils::NTPL((const char *)"define IPv6_SrcAddr = Field(Layer3Header[8]/128)");
    ntplIdx[ntplIdxCnt++] = NTUtils::NTPL((const char *)"define IPv6_DstAddr = Field(Layer3Header[24]/128)");


    string streamRange;
    
if (streamid.length() > 3) {
    streamRange = "(";
    streamRange += streamid;
    streamRange += ")";    
}   else {
    streamRange = streamid;
} 
//    string range;
//    range << "(" << "0..3" << ")";
    //printf("range: %s\n", streamRange.c_str());
//            
    
    // Set up streams to handle IPv4/IPv6 source only OR destination only addresses in the TCAM using KeyMatch
    // Streams where either source or destination are handled are handled via the CAM IPMatch filters.
    
    // Match on IPv4 Source only
    ntplIdx[ntplIdxCnt++] = NTUtils::NTPL((const char *)
            "assign[streamid=%s;priority=%d;color=%d] = (Layer3Protocol == IPv4) AND (KeyMatch(IPv4_SrcAddr) == %d AND KeyMatch(IPv4_DstAddr) != %d)", 
            streamRange.c_str(),
            opt_priority,
            opt_color,
            IPAddress::MatchKeyIPv4Src, IPAddress::MatchKeyIPv4Src);
    
    // Match on IPv4 Destination only
    ntplIdx[ntplIdxCnt++] = NTUtils::NTPL((const char *)
            "assign[streamid=%s;priority=%d;color=%d] = (Layer3Protocol == IPv4) AND (KeyMatch(IPv4_SrcAddr) != %d AND KeyMatch(IPv4_DstAddr) == %d)", 
            streamRange.c_str(), 
            opt_priority+1,
            opt_color,
            IPAddress::MatchKeyIPv4Dst, IPAddress::MatchKeyIPv4Dst);

    // Match on IPv6 Source only
    ntplIdx[ntplIdxCnt++] = NTUtils::NTPL((const char *)
            "assign[streamid=%s;priority=%d;color=%d] = (Layer3Protocol == IPv6) AND (KeyMatch(IPv6_SrcAddr) == %d AND KeyMatch(IPv6_DstAddr) != %d)", 
            streamRange.c_str(),
            opt_priority,
            opt_color,
            IPAddress::MatchKeyIPv6Src, IPAddress::MatchKeyIPv6Src);
    
    // Match on IPv6 Destination only
    ntplIdx[ntplIdxCnt++] = NTUtils::NTPL((const char *)
            "assign[streamid=%s;priority=%d;color=%d] = (Layer3Protocol == IPv6) AND (KeyMatch(IPv6_SrcAddr) != %d AND KeyMatch(IPv6_DstAddr) == %d)", 
            streamRange.c_str(), 
            opt_priority + 1,
            opt_color,
            IPAddress::MatchKeyIPv6Dst, IPAddress::MatchKeyIPv6Dst);
    
    ntplIdx[ntplIdxCnt++] = NTUtils::NTPL((const char *)
            "assign[streamid=%s;priority=%d;color=%d] = IPMatch == SrcIP,DstIP", 
            streamRange.c_str(), 
            opt_priority + 2,
            opt_color
            );        

   
    
    if (fHandler.open(filename)) {
        while (applicationRunning) {
            if (fHandler.watchFile(&applicationRunning)) {
                fHandler.process();
            }
        }
    }

    //-----    -----    -----    -----    -----    -----    -----    -----
    // shutdown/cleanup    
    fHandler.close();
    
    NTUtils::log(LOG_INFO, "deleting filters:\n");
    for (i = 0; i < ntplIdxCnt; ++i) {
        if (ntplIdx[i]) {
            NTUtils::NTPL((const char *) "delete=%d", ntplIdx[i]);
        }
    }

    NTUtils::log(LOG_NOTICE, "Exiting Napatech Filter daemon.\n");
    
    closelog();
}



