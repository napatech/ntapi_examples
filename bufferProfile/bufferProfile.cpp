


#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>

#include <getopt.h>
#include <nt.h>

#include "bufferProfile.h"
#include "backlog.h"

//#include <wjelement.h>
//#include <libwebsockets.h>

//#include "../include/websockports.h"
//#include "../NTWebSocket/NTWebSocket.h"


int g_appRunning = 1;
//uint64_t g_maxUtil = 0;
#define MAX_PORTS 32

#define OPTION_HELP             (1<<1)

#define OPTION_SHORT_HELP       'h'

#define OPTION_FILE             'f'
static uint32_t opt_filename = false;

#define OPTION_DAEMON            'd'
static uint32_t opt_daemon = false;

#define OPTION_CONTINUOUS        'c'
static uint32_t opt_continuous = false;

#define OPTION_KILL             'k'

/**
 * Table of valid options. Mainly used for the getopt_long_only
 * function. For further info see the manpage.
 */
static struct option long_options[] = {
    {"help", no_argument, 0, OPTION_HELP},
    {0, 0, 0, 0}
};

#define MAX_CPU 128
#define CPU_ENTRIES 12
#define MAX_STREAMS 256
#define HB_HIGHWATER 991

typedef struct hbStats_s {
    uint32_t adapterNo;
    uint32_t obFillLevel; // On-board Fill level
    uint32_t hbFillLevel; // Host Buffer Fill Level
    uint32_t numaNode;

    uint64_t rxPktsPrev;
    uint64_t rxPktsTotal;
    uint64_t dropPrev;
    uint64_t dropTotal;
    uint64_t rxBytesPrev;
    uint64_t rxBytesTotal;
} hbStats_t;

typedef struct streamStats_s {
    bool alert;
    struct hbStats_s hb[2];
    uint64_t dropCurr;
    uint64_t rxPktsCurr;
    uint64_t rxBytesCurr;
    uint32_t rxPktsRate;
    uint32_t aveOBFillLevel;
    uint32_t aveHBFillLevel;
} streamStats_t;

typedef struct sysStats_s {
    uint64_t time;
    uint32_t numCPU;
    uint32_t numStreams;
    uint64_t CPUutil[MAX_CPU];
    streamStats_t streamStats[MAX_STREAMS];
} sysStats_t;


static void StopApplication(int sig) {
    printf("\nStopping application (%d)\n", sig);
    g_appRunning = 0;
}

static void _Usage(void) {
    fprintf(stderr, "%s\n", usageText);
    exit(1);
}


static CBacklog backlog;


static char *decodeTimeStamp(uint64_t ts, char *ts_buf) {
    time_t tmp_ts;
    struct tm *loc_time;
    //  char ts_buf[256];

    tmp_ts = (time_t) (ts / 100000000);
    loc_time = localtime(&tmp_ts);
    sprintf(ts_buf, "%04d/%02d/%02d-%02d:%02d:%02d.%09lu",
            loc_time->tm_year + 1900, loc_time->tm_mon + 1, loc_time->tm_mday,
            loc_time->tm_hour, loc_time->tm_min, loc_time->tm_sec,
            (unsigned long) (ts % 100000000 * 10));
    //printf("0x%lx: %s\n", ts, ts_buf);
    return ts_buf;
}


char * sysExec(char *cmd, char *result, uint32_t maxResult) {

    FILE* myPipe = popen(cmd, "r");
    memset(result, 0, maxResult);

    if (!myPipe) return NULL;

    char buffer[1024];
    while (!feof(myPipe)) {
        if (fgets(buffer, 1024, myPipe) != NULL) {
            if ((strlen(result) + strlen(buffer)) > maxResult) {
                printf("ERROR: sysExec %s - buffer too small.\n", cmd);
                return NULL;
            }
            strcat(result, buffer);
        }
    }
    pclose(myPipe);
    return result;

}


#define ALL_LINE_OFFSET 3
#define ALL_ZERO_OFFSET 4
#define TOP_START_LINE 7

#define ASCII_NULL 0
#define ASCII_ESC 27
#define ASCII_SPACE 32
#define ASCII_TAB 9
#define ASCII_LF 12
#define ASCII_DEL 127
#define ASCII_0  48
#define ASCII_9  57

char *getLine(uint32_t line, char *buf) {
    uint32_t i;
    char *p = buf;
    for (i = 0; i < line; ++i) {
        p = strchr(p + 1, '\n');

        if (p == NULL) return NULL;
    }
    ++p;

    while ((p != NULL) && (*p != 0) && ((*p < ASCII_0) || (*p > ASCII_9))) {
        ++p;
        //        printf("+");
    }
    return p;
}

void copyField(char *p, char *buf, uint32_t bufSize) {
    // Copy the field to the user buffer
    uint32_t pos = 0;
    char *p2;
    p2 = p;
    while (!(*p2 == ' ' || *p2 == '\t' || *p2 == '\n' || *p2 == '\r' || *p2 == ASCII_ESC)) {
        buf[pos] = *p2;
        if (++pos >= bufSize - 1) break;
        ++p2;
    }
    buf[pos] = 0;

}

char *getNextField(char *p, char *buf, uint32_t bufSize) {
    uint32_t failSafeCnt = 0;

    if (p == NULL) {
        printf("WARNING - getNextField() has NULL pointer");
        return NULL;
    }

    while (((*p > ASCII_SPACE) && (*p < ASCII_DEL))
            || (*p == ASCII_ESC)
            //|| (*p == ASCII_NULL ) 
            ) {

        if (failSafeCnt++ > 1024) {
            printf("failSafe - *%d*\n", *p);
            return NULL;
        }
        ++p;
    }

    while (*p == ' ' || *p == '\t') {
        ++p;
    }

    // Copy the field to the user buffer
    copyField(p, buf, bufSize);

    if (*p == '\n' || *p == '\r' || *p == ASCII_ESC) return NULL;

    return p;
}

void printLine(char *p) {
    char *p1 = p;

    printf("|||>");
    for (uint32_t i = 0; i < 96; ++i) {

        if (p1 == NULL) {
            break;
        }
        printf("%c", *p1);
        ++p1;
    }
    printf("<|||");
}

void printField(char *p) {
    char *p1 = p;

    for (uint32_t i = 0; i < 12; ++i) {

        if (p1 == NULL) {
            break;
        }
        printf("%c", *p1);
        ++p1;
    }
    //    printf("\n");
}

char *getField(uint32_t fieldID, char *p, char *buf, uint32_t bufSize) {
    //    char tBuf[256];

    for (uint32_t i = 0; i < fieldID; ++i) {
        memset(buf, 0, bufSize);
        p = getNextField(p, buf, bufSize);
        //printField(p);

        if (p == NULL) {
            break;
        }
    }
    return p;
}



uint32_t readStreamStats(streamStats_t streamStats[], NtInfoStream_t hInfo, NtStatStream_t hStatStream, bool *alert) {
    NtInfo_t hStreamInfo;
    NtStatistics_t hStat; // Stat handle.

    char errorBuffer[NT_ERRBUF_SIZE]; // Error buffer
    int status; // Status variable
    uint32_t hbCount;
    static bool firstTime = true;
    //    bool backupDetected = false;


    // Read the info on all streams instantiated in the system
    hStreamInfo.cmd = NT_INFO_CMD_READ_STREAM;
    if ((status = NT_InfoRead(hInfo, &hStreamInfo)) != NT_SUCCESS) {
        NT_ExplainError(status, errorBuffer, sizeof (errorBuffer) - 1);
        fprintf(stderr, "NT_InfoRead() failed: %s\n", errorBuffer);
        return 0;
    }

    //    uint64_t totalCnt[MAX_STREAMS] = {0};
    //    uint64_t dropCnt[MAX_STREAMS] = {0};
    char pktCntStr[4096];
    memset(pktCntStr, 0, sizeof (pktCntStr));

    uint32_t stream_id = 0;
    uint32_t stream_cnt = 0;
    uint32_t numStreams = hStreamInfo.u.stream.data.count;
    //    printf("numStreams: %d\n", numStreams);
    //    printf("found stats for stream(s) ", stream_id);

    for (stream_cnt = 0; stream_cnt < numStreams; ++stream_cnt) {

        do {
            // Read usage data for the chosen stream ID
            hStat.cmd = NT_STATISTICS_READ_CMD_USAGE_DATA_V0;
            hStat.u.usageData_v0.streamid = (uint8_t) stream_id;
//            hStat.u.query_v2.clear = 1;
//            hStat.u.query_v1.clear = 1;

            if ((status = NT_StatRead(hStatStream, &hStat)) != NT_SUCCESS) {
                // Get the status code as text
                NT_ExplainError(status, errorBuffer, sizeof (errorBuffer));
                fprintf(stderr, "NT_StatRead() failed: %s\n", errorBuffer);
                return 0;
            }

            if (hStat.u.usageData_v0.data.numHostBufferUsed == 0) {
                ++stream_id;
                continue;
            }
        } while (hStat.u.usageData_v0.data.numHostBufferUsed == 0);


        streamStats[stream_id].aveOBFillLevel = 0;
        streamStats[stream_id].aveHBFillLevel = 0;
        
        for (hbCount = 0; hbCount < hStat.u.usageData_v0.data.numHostBufferUsed; hbCount++) {

            streamStats[stream_id].hb[hbCount].adapterNo = hStat.u.usageData_v0.data.hb[hbCount].adapterNo;
            streamStats[stream_id].hb[hbCount].numaNode = hStat.u.usageData_v0.data.hb[hbCount].numaNode;
            streamStats[stream_id].hb[hbCount].obFillLevel = (uint32_t) 
                    ((100 * hStat.u.usageData_v0.data.hb[hbCount].onboardBuffering.used) /
                    hStat.u.usageData_v0.data.hb[hbCount].onboardBuffering.size);
            
            uint32_t bufSize =  hStat.u.usageData_v0.data.hb[hbCount].enQueuedAdapter/1024
                              + hStat.u.usageData_v0.data.hb[hbCount].deQueued/1024
                              + hStat.u.usageData_v0.data.hb[hbCount].enQueued/1024
                              - HB_HIGHWATER;
            
            streamStats[stream_id].hb[hbCount].hbFillLevel = (uint32_t)                                 
                                ((100 * hStat.u.usageData_v0.data.hb[hbCount].deQueued/1024) /
                                bufSize);
            
            streamStats[stream_id].hb[hbCount].rxPktsPrev = streamStats[stream_id].hb[hbCount].rxPktsTotal;
            streamStats[stream_id].hb[hbCount].rxBytesPrev = streamStats[stream_id].hb[hbCount].rxBytesTotal;
            streamStats[stream_id].hb[hbCount].dropPrev = streamStats[stream_id].hb[hbCount].dropTotal;

            streamStats[stream_id].hb[hbCount].rxPktsTotal = hStat.u.usageData_v0.data.hb[hbCount].stat.rx.frames;
            streamStats[stream_id].hb[hbCount].rxBytesTotal = hStat.u.usageData_v0.data.hb[hbCount].stat.rx.bytes;
            
            streamStats[stream_id].hb[hbCount].dropPrev = streamStats[stream_id].hb[hbCount].dropTotal;
            streamStats[stream_id].hb[hbCount].dropTotal = hStat.u.usageData_v0.data.hb[hbCount].stat.drop.frames;

            streamStats[stream_id].alert =
                     //(streamStats[stream_id].hb[hbCount].obFillLevel > 90
                     (streamStats[stream_id].hb[hbCount].dropTotal - streamStats[stream_id].hb[hbCount].dropPrev > 0);

            streamStats[stream_id].rxPktsCurr = streamStats[stream_id].hb[hbCount].rxPktsTotal
                                              - streamStats[stream_id].hb[hbCount].rxPktsPrev;
            
            
            streamStats[stream_id].rxBytesCurr = streamStats[stream_id].hb[hbCount].rxBytesTotal 
                                               - streamStats[stream_id].hb[hbCount].rxBytesPrev;
            
            streamStats[stream_id].dropCurr = streamStats[stream_id].hb[hbCount].dropTotal
                    - streamStats[stream_id].hb[hbCount].dropPrev;

            streamStats[stream_id].aveOBFillLevel += streamStats[stream_id].hb[hbCount].obFillLevel;
            streamStats[stream_id].aveHBFillLevel += streamStats[stream_id].hb[hbCount].hbFillLevel;
        }
        

        if (!firstTime && streamStats[stream_id].alert) {
            *alert = true;        
        }

        streamStats[stream_id].aveOBFillLevel /= hStat.u.usageData_v0.data.numHostBufferUsed;
        streamStats[stream_id].aveHBFillLevel /= hStat.u.usageData_v0.data.numHostBufferUsed;
        
        ++stream_id;
    }
    firstTime = false;

    //    printf("\n");


    return numStreams;
}

double time_so_far(void) {
    struct timeval tp;

    if (gettimeofday(&tp, (struct timezone *) NULL) == -1)
        perror("gettimeofday");
    return ((double) (tp.tv_sec)) +
            (((double) tp.tv_usec) * 0.000001);
}



uint32_t readCPUStats(uint64_t *util) {
    FILE *f1;
    char c[10];
    uint64_t time;

    uint64_t cpu[MAX_CPU][CPU_ENTRIES];
    uint64_t t[MAX_CPU];

    static uint32_t numCPU;
    static uint64_t prevTime;
    static uint64_t prevCpu[MAX_CPU][CPU_ENTRIES];

    if (util == NULL) {
        printf("Initializing CPU Stats...");

        numCPU = sysconf(_SC_NPROCESSORS_ONLN);
    }

    // Read in proc data
    time = time_so_far();
    f1 = fopen("/proc/stat", "r");
    for (uint32_t i = 0; i < numCPU + 1; i++) {
        fscanf(f1, "%s\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\n", c,
                &cpu[i][0], &cpu[i][1], &cpu[i][2], &cpu[i][3], &cpu[i][4], &cpu[i][5], &cpu[i][6], &cpu[i][7], &cpu[i][8], &cpu[i][9], &cpu[i][10]);
    }
    fclose(f1);

    // Calculate utilization since last read
    if (util != NULL) {

        for (uint32_t i = 0; i < numCPU + 1; i++) {
            t[i] = (cpu[i][0] + cpu[i][1] + cpu[i][2])-(prevCpu[i][0] + prevCpu[i][1] + prevCpu[i][2]);
            util[i] = (uint64_t) (t[i] / ((time - prevTime)));
        }
    } else {// first_time == true
        printf("done\n");
    }

    prevTime = time;
    for (uint32_t i = 0; i < numCPU + 1; i++) {
        for (uint32_t j = 0; j <= 10; j++) {
            prevCpu[i][j] = cpu[i][j];
        }
    }
    return numCPU;
}

char * formatRate(uint64_t BytesPerSec, char *rateBuf) {
    if (8*BytesPerSec/1000 < 1000000) {
        sprintf(rateBuf, "%8.3f", (float)8*BytesPerSec/1000000000);      
    } else if (8*BytesPerSec/1000 < 10000000) {
        sprintf(rateBuf, "%8.2f", (float)8*BytesPerSec/1000000000);              
    } else {
        sprintf(rateBuf, "%8.1f", (float)8*BytesPerSec/1000000000);
    }
    return rateBuf;
}


void outputStats(
            sysStats_t sysStats,
            uint64_t numPorts,
            uint64_t rxMbps[MAX_PORTS],
            bool alert) {
    uint32_t i;
    static bool prevAlert;
    //    char errorBuffer[2048];

    char time[2048];
    char numStreams[8];
    char pktsStr[2048];
    char pktsRateStr[2048];
    char dropStr[2048];
    char obFillStr[2048];
    char hbFillStr[2048];
    char rxMbpsStr[2048];
    char rxUtilStr[2048];

    memset(rxMbpsStr, 0, sizeof (rxMbpsStr));
    memset(rxUtilStr, 0, sizeof (rxUtilStr));
    memset(pktsStr, 0, sizeof (pktsStr));
    memset(pktsRateStr, 0, sizeof (pktsRateStr));
    memset(dropStr, 0, sizeof (dropStr));
    memset(obFillStr, 0, sizeof (obFillStr));
    memset(hbFillStr, 0, sizeof (hbFillStr));

    //    printf("------------------------------------+\n");
    sprintf(time, "%s\n", decodeTimeStamp(sysStats.time, time));

    for (i = 0; i < numPorts; ++i) {
        char temp[2048];
        snprintf(temp, 2048, "%s", rxMbpsStr);
        snprintf(rxMbpsStr, 2048, "%s %8ld ", temp, rxMbps[i]);        
    }    

    sprintf(numStreams, "%d", sysStats.numStreams);
    
    for (i = 0; i < sysStats.numStreams; ++i) {       
        char temp[2048];
        snprintf(temp, 2048, "%s", pktsStr);
        if ((i % 8) == 7) {
            snprintf(pktsStr, 2048, "%s %8ld\n", temp, sysStats.streamStats[i].rxPktsCurr);
        } else {
            snprintf(pktsStr, 2048, "%s %8ld ", temp, sysStats.streamStats[i].rxPktsCurr);
        }
        
        snprintf(temp, 2048, "%s", pktsRateStr);
        char rateBuf[64];
        if ((i % 8) == 7) {
            snprintf(pktsRateStr, 2048, "%s %s\n", temp, formatRate(sysStats.streamStats[i].rxBytesCurr, rateBuf));
        } else {
            snprintf(pktsRateStr, 2048, "%s %s ", temp, formatRate(sysStats.streamStats[i].rxBytesCurr, rateBuf));
        }
        
        snprintf(temp, 2048, "%s", dropStr);
        if ((i % 8) == 7) {
            snprintf(dropStr, 2048, "%s %8ld\n", temp, sysStats.streamStats[i].dropCurr);
        } else {
            snprintf(dropStr, 2048, "%s %8ld ", temp, sysStats.streamStats[i].dropCurr);
        }

        snprintf(temp, 2048, "%s", obFillStr);
        if ((i % 8) == 7) {
            snprintf(obFillStr, 2048, "%s %8d\n", temp, sysStats.streamStats[i].aveOBFillLevel);
        } else {
            snprintf(obFillStr, 2048, "%s %8d ", temp, sysStats.streamStats[i].aveOBFillLevel);
        }

        snprintf(temp, 2048, "%s", hbFillStr);
        if ((i % 8) == 7) {
            snprintf(hbFillStr, 2048, "%s %8d\n", temp, sysStats.streamStats[i].aveHBFillLevel);
        } else {
            snprintf(hbFillStr, 2048, "%s %8d ", temp, sysStats.streamStats[i].aveHBFillLevel);
        }
    }
    
//    printf("pktsRateStr: %s\n", pktsRateStr);
//    printf("hbuffUtil: %s\n", hbFillStr);
//    printf("sdramUtil: %s\n", obFillStr);
//    printf("drop:      %s\n", dropStr);

//    printf("\n");
    
    char utilBuf[4096];
    memset(utilBuf, 0, sizeof (utilBuf));

    for (i = 1; i < sysStats.numCPU + 1; ++i) {
        char temp[2048];
        snprintf(temp, 2048, "%s", utilBuf);
        if ((i % 16) == 0) {
            sprintf(utilBuf, "%s %4ld\n", temp, sysStats.CPUutil[i]);
        } else {
            sprintf(utilBuf, "%s %4ld ", temp, sysStats.CPUutil[i]);
        }
    }

    backlog.addEntry(
            time,
            rxMbpsStr,
            numStreams,
            pktsStr,
            pktsRateStr,
            dropStr,
            hbFillStr,
            obFillStr,
            utilBuf
            );

    if (alert || prevAlert) {
//        printf("---- ALERT ----\n");
        backlog.output(prevAlert && !alert);
    }
    
    prevAlert = alert;
}

static int getPortSpeed(uint64_t portSpeed[], uint32_t *_numPorts, char *errorBuffer) {
    NtInfo_t hInfo;
    NtInfoStream_t hInfoStream;
    int status;

    // Open the info stream.
    if ((status = NT_InfoOpen(&hInfoStream, "ExampleInfo")) != NT_SUCCESS) {
        NT_ExplainError(status, errorBuffer, sizeof (errorBuffer) - 1);
        fprintf(stderr, "NT_InfoOpen() failed: %s\n", errorBuffer);
        return -1;
    }

    // Read the system info
    hInfo.cmd = NT_INFO_CMD_READ_SYSTEM;
    if ((status = NT_InfoRead(hInfoStream, &hInfo)) != NT_SUCCESS) {
        NT_ExplainError(status, errorBuffer, sizeof (errorBuffer) - 1);
        fprintf(stderr, "NT_InfoRead() failed: %s\n", errorBuffer);
        return -1;
    }

    int numPorts = hInfo.u.system.data.numPorts;
    int port;
    for (port = 0; port < numPorts; ++port) {
        // Perform per port checks
        hInfo.cmd = NT_INFO_CMD_READ_PORT;
        hInfo.u.port.portNo = port;
        if ((status = NT_InfoRead(hInfoStream, &hInfo)) != 0) {
            NT_ExplainError(status, errorBuffer, sizeof (errorBuffer) - 1);
            fprintf(stderr, "NT_InfoRead failed: %s", errorBuffer);
            NT_InfoClose(hInfoStream);
            return status;
        }

        switch (hInfo.u.port.data.speed) {
            case NT_LINK_SPEED_10M:
                portSpeed[port] = 10;
                break;
            case NT_LINK_SPEED_100M:
                portSpeed[port] = 100;
                break;
            case NT_LINK_SPEED_1G:
                portSpeed[port] = 1000;
                break;
            case NT_LINK_SPEED_10G:
                portSpeed[port] = 10000;
                break;
            case NT_LINK_SPEED_40G:
                portSpeed[port] = 40000;
                break;
            case NT_LINK_SPEED_100G:
                portSpeed[port] = 100000;
                break;
            default:
                portSpeed[port] = 0;
                break;
        }
    }

    // Close the info stream
    if ((status = NT_InfoClose(hInfoStream)) != NT_SUCCESS) {
        NT_ExplainError(status, errorBuffer, sizeof (errorBuffer) - 1);
        fprintf(stderr, "NT_InfoClose() failed: %s\n", errorBuffer);
        return -1;
    }
    *_numPorts = numPorts;
    return 0;
}

static int getPortsStats(NtStatStream_t hStatStream,
                         uint32_t numPorts,
                         uint64_t portSpeed[],
                         uint64_t rxPkts[],
                         uint64_t rxDropped[],
                         uint64_t rxUtilization[],
                         uint64_t rxMbps[],
                         char *errorBuffer) {
    uint32_t port;
    int status; // Status variable
    NtStatistics_t hStat; // Stat handle.
    uint64_t rxBytes[MAX_PORTS];

    // Open the stat stream.
    hStat.cmd = NT_STATISTICS_READ_CMD_QUERY_V1;
    hStat.u.query.poll = 1; // The the current counters
    hStat.u.query.clear = 1;
    if ((status = NT_StatRead(hStatStream, &hStat)) != NT_SUCCESS) {
        // Get the status code as text
        NT_ExplainError(status, errorBuffer, sizeof (errorBuffer) - 1);
        fprintf(stderr, "NT_StatRead() 2 - failed: %s\n", errorBuffer);
        exit(-1);
    }

    for (port = 0; port < numPorts; ++port) {
        if (hStat.u.query.data.port.aPorts[port].rx.valid.RMON1) {
            rxPkts[port] = (unsigned long long) hStat.u.query.data.port.aPorts[port].rx.RMON1.pkts;
            rxBytes[port] = (unsigned long long) hStat.u.query.data.port.aPorts[port].rx.RMON1.octets;
            rxDropped[port] = (unsigned long long) hStat.u.query.data.port.aPorts[port].rx.extDrop.pktsOverflow;

            // Calulate throughput in Mbps
#define ENET_OVERHEAD 20.0

            int64_t dT = 100000000;
            int64_t dRx_pkts = rxPkts[port]; // - prev_rxPkts[port];

            if (dT > 0) {
                rxMbps[port] = (800.0 * ((dRx_pkts * ENET_OVERHEAD) + (rxBytes[port]))) / dT;
            }

            if (portSpeed[port] > 0) {
                rxUtilization[port] = (unsigned long long) (100.0 * (double) rxMbps[port] / (double) portSpeed[port]);
            } else {
                rxUtilization[port] = 0;
            }
        }
    }

    return 0;
}

#define PIDFILE "/var/run/bufferProfile.pid"

bool writePIDFile(pid_t pid) {
    FILE* fd = NULL;
    fd = fopen(PIDFILE,"w");
    rewind(fd);
   
    if (NULL != fd) {
        fprintf(fd, "%d", pid);
        fclose(fd);
        return true;
    } else {
//        NTUtils::log(LOG_ERR, "Failed to open PIDFile!");
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
    printf("PID: %d\n", pid);
    return pid;
}


int main(int argc, char**argv) {
    //int main() {
    //    FILE *f1;
    char *p = argv[0];
    int numOptions = 0;
    int option;
    int option_index = 0;
    char filename[256];

    sysStats_t sysStats;

    uint32_t i = 0;
    int status;
    char errorBuffer[80]; // Error buffer
    NtInfoStream_t hInfo;
    NtStatStream_t hStatStream;
    uint32_t numPorts = 0;
    uint64_t portSpeed[MAX_PORTS];

    signal(SIGINT, StopApplication);

    //-----    -----    -----    -----    -----    -----    -----    -----
    // Process command line options
    // Strip path
    for (; i != 0; i--) {
        if ((p[i - 1] == '.') || (p[i - 1] == '\\') || (p[i - 1] == '/')) {
            break;
        }
    }
    int res;
    while ((option = getopt_long(argc, argv, "f:dkch", long_options, &option_index)) != -1) {
        numOptions++;
        switch (option) {
            case OPTION_HELP:
            case OPTION_SHORT_HELP:
                _Usage();
                break;
            case OPTION_FILE:
                sprintf(filename, "%s", optarg);
                opt_filename = true;
                printf("OPTION_FILE: %s\n", filename);
                break;
            case OPTION_KILL:
            {
                pid_t pid = readPIDFile();
                if (pid != 0) {
                    res = kill(pid, SIGUSR1);
                    if (res != -1) {
                        printf("Running Instance terminated.\n");
                    } else {
                        printf("Couldn't kill the process!  Cleaning up...\n");
                    }
                    writePIDFile(0);
                } else {
                    printf("No Running Instances found.\n");                    
                }
                exit(0);
                break;
            }
                
            case OPTION_DAEMON:
                if (readPIDFile() == 0) {
                    opt_daemon = true;
                } else {
                    printf("Error: Running instance detected.  Please use -k option to kill it.\n");
                    exit(-1);
                }
                break;

            case OPTION_CONTINUOUS:
                opt_continuous = true;
                break;

            default:
                printf("ERROR: Invalid command line option!!!");
                //invalidOption = 1;
                return -1;
                break;
        }
    }

    {
            string fname = filename;
            char ts_buf[256];
            struct timeval tv;
            gettimeofday(&tv, NULL);

            uint64_t time = ((tv.tv_sec * 1000000) + tv.tv_usec)*100;
        if (opt_filename) {       
            backlog.initialize(fname, decodeTimeStamp(time, ts_buf));
        } else if (!opt_daemon) {
            backlog.initialize("", decodeTimeStamp(time, ts_buf));
        } else {
            _Usage();
        }
    }

    //-----    -----    -----    -----    -----    -----    -----    -----
    // Initialize the NTAPI library
    if ((status = NT_Init(NTAPI_VERSION)) != NT_SUCCESS) {
        NT_ExplainError(status, errorBuffer, sizeof (errorBuffer) - 1);
        fprintf(stderr, "NT_Init() failed: %s\n", errorBuffer);
        return status;
    }

    // Open the info and Statistics
    if ((status = NT_InfoOpen(&hInfo, "InfoStream")) != NT_SUCCESS) {
        NT_ExplainError(status, errorBuffer, sizeof (errorBuffer) - 1);
        fprintf(stderr, "NT_InfoOpen() failed: %s\n", errorBuffer);
        return -1;
    }

    if ((status = NT_StatOpen(&hStatStream, "StatsStream")) != NT_SUCCESS) {
        // Get the status code as text
        NT_ExplainError(status, errorBuffer, sizeof (errorBuffer));
        fprintf(stderr, "NT_StatOpen() failed: %s\n", errorBuffer);
        return -1;
    }

    if (getPortSpeed(portSpeed, &numPorts, errorBuffer) != NT_SUCCESS) {
        return -1;
    }


    if (opt_daemon) {
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
        printf("bufferUtil daemon process ID: %d\n", sid);
        
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
     

    uint32_t numCPU = sysconf(_SC_NPROCESSORS_ONLN);
    printf("numCPU: %d\n", numCPU);

    readCPUStats(NULL); //initialize

    while (g_appRunning) {
        uint64_t rxPkts[MAX_PORTS];
        uint64_t rxDropped[MAX_PORTS];
        uint64_t rxMbps[MAX_PORTS];
        uint64_t rxUtilization[MAX_PORTS];

        usleep(1000000);
        struct timeval tv;
        gettimeofday(&tv, NULL);
        bool alert = false;

        sysStats.time = ((tv.tv_sec * 1000000) + tv.tv_usec)*100;
        sysStats.numCPU = readCPUStats(sysStats.CPUutil);
        sysStats.numStreams = readStreamStats(sysStats.streamStats, hInfo, hStatStream, &alert);

        if (getPortsStats(hStatStream,
                numPorts, portSpeed,
                rxPkts, rxDropped, rxUtilization, rxMbps,
                errorBuffer) == -1) {
            return -1;
        }

        outputStats(sysStats, numPorts, rxMbps, (opt_continuous || alert));
    } // end while()

    backlog.write("Exit Loop!\n");

    printf("Shutting down...\n");
    // CLEAN UP NT Resources
    // Close the info stream
    if ((status = NT_InfoClose(hInfo)) != NT_SUCCESS) {
        NT_ExplainError(status, errorBuffer, sizeof (errorBuffer) - 1);
        fprintf(stderr, "NT_InfoClose() failed: %s\n", errorBuffer);
        return -1;
    }
    
    printf("Info closed\n");
    // Close the statistics stream
    if ((status = NT_StatClose(hStatStream)) != NT_SUCCESS) {
        // Get the status code as text
        NT_ExplainError(status, errorBuffer, sizeof (errorBuffer));
        fprintf(stderr, "NT_StatClose() failed: %s\n", errorBuffer);
        return -1;
    }
    printf("Stat closed\n");

    NT_Done();
    {    
        char ts_buf[256];
        struct timeval tv;
        gettimeofday(&tv, NULL);

        uint64_t time = ((tv.tv_sec * 1000000) + tv.tv_usec)*100;
        backlog.shutdown(decodeTimeStamp(time, ts_buf));
    }
    
    printf("Done.\n");
}
