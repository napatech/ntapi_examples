/*
 * Copyright 2022 Napatech A/S. All rights reserved.
 * 
 * 1. Copying, modification, and distribution of this file, or executable
 * versions of this file, is governed by the terms of the Napatech Software
 * license agreement under which this file was made available. If you do not
 * agree to the terms of the license do not install, copy, access or
 * otherwise use this file.
 * 
 * 2. Under the Napatech Software license agreement you are granted a
 * limited, non-exclusive, non-assignable, copyright license to copy, modify
 * and distribute this file in conjunction with Napatech SmartNIC's and
 * similar hardware manufactured or supplied by Napatech A/S.
 * 
 * 3. The full Napatech Software license agreement is included in this
 * distribution, please see "NP-0405 Napatech Software license
 * agreement.pdf"
 * 
 * 4. Redistributions of source code must retain this copyright notice,
 * list of conditions and the following disclaimer.
 * 
 * THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTIES, EXPRESS OR
 * IMPLIED, AND NAPATECH DISCLAIMS ALL IMPLIED WARRANTIES INCLUDING ANY
 * IMPLIED WARRANTY OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, OR OF
 * FITNESS FOR A PARTICULAR PURPOSE. TO THE EXTENT NOT PROHIBITED BY
 * APPLICABLE LAW, IN NO EVENT SHALL NAPATECH BE LIABLE FOR PERSONAL INJURY,
 * OR ANY INCIDENTAL, SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES WHATSOEVER,
 * INCLUDING, WITHOUT LIMITATION, DAMAGES FOR LOSS OF PROFITS, CORRUPTION OR
 * LOSS OF DATA, FAILURE TO TRANSMIT OR RECEIVE ANY DATA OR INFORMATION,
 * BUSINESS INTERRUPTION OR ANY OTHER COMMERCIAL DAMAGES OR LOSSES, ARISING
 * OUT OF OR RELATED TO YOUR USE OR INABILITY TO USE NAPATECH SOFTWARE OR
 * SERVICES OR ANY THIRD PARTY SOFTWARE OR APPLICATIONS IN CONJUNCTION WITH
 * THE NAPATECH SOFTWARE OR SERVICES, HOWEVER CAUSED, REGARDLESS OF THE THEORY
 * OF LIABILITY (CONTRACT, TORT OR OTHERWISE) AND EVEN IF NAPATECH HAS BEEN
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGES. SOME JURISDICTIONS DO NOT ALLOW
 * THE EXCLUSION OR LIMITATION OF LIABILITY FOR PERSONAL INJURY, OR OF
 * INCIDENTAL OR CONSEQUENTIAL DAMAGES, SO THIS LIMITATION MAY NOT APPLY TO YOU.
 */

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <ncurses.h>
#include <getopt.h>
#include <nt.h>

static const char usageText[] =
        "\nDisplays a histogram representing host buffer and on-board SDRAM usage\n"
        "for each stream that is configured in the system.\n"
        "Syntax:\n"
        "  hbMonitor [--help][-h]\n"
        "\n"
        "\n  --help, -h      : Displays the help.\n";

int g_appRunning = 1;


#define OPTION_HELP             (1<<1)
#define OPTION_SHORT_HELP       'h'

/**
 * Table of valid options. Mainly used for the getopt_long_only
 * function. For further info see the manpage.
 */
static struct option long_options[] = {
    {"help", no_argument, 0, OPTION_HELP},
    {0, 0, 0, 0}
};

#define MAX_STREAMS 256
//#define HB_HIGHWATER 991
#define HB_HIGHWATER 2048 //991

typedef struct hbStats_s {
    uint64_t obFillLevel; // On-board Fill level
    uint64_t hbFillLevel; // Host Buffer Fill Level
    uint64_t pktsPrev;
    uint64_t pktsTotal;
    uint64_t dropPrev;
    uint64_t dropTotal;
} hbStats_t;

typedef struct streamStats_s {
    struct hbStats_s hb[2];
    bool active;
    uint64_t pktsCurr;
    uint64_t dropCurr;
    uint32_t aveOBFillLevel;
    uint32_t aveHBFillLevel;
    uint32_t adapterNo;
} streamStats_t;

typedef struct sysStats_s {
    uint64_t time;
    uint32_t numStreams;
    streamStats_t streamStats[MAX_STREAMS];
} sysStats_t;

static void StopApplication(int sig) {
    g_appRunning = 0;
    endwin(); /* End curses mode		  */
    exit(sig);
}

static void _Usage(void) {
    fprintf(stderr, "%s\n", usageText);
    exit(1);
}

static char *decodeTimeStamp(uint64_t ts, char *ts_buf) {
    time_t tmp_ts;
    struct tm *loc_time;
    //  char ts_buf[256];

    tmp_ts = (time_t) (ts / 100000000);
    loc_time = localtime(&tmp_ts);
    sprintf(ts_buf, "%04d/%02d/%02d-%02d:%02d:%02d",
            loc_time->tm_year + 1900, loc_time->tm_mon + 1, loc_time->tm_mday,
            loc_time->tm_hour, loc_time->tm_min, loc_time->tm_sec);
    return ts_buf;
}

uint32_t readStreamStats(streamStats_t streamStats[], NtInfoStream_t hInfo, NtStatStream_t hStatStream) {
    NtInfo_t hStreamInfo;
    NtStatistics_t hStat; // Stat handle.

    char errorBuffer[NT_ERRBUF_SIZE]; // Error buffer
    int status; // Status variable
    uint32_t hbCount;

    // Read the info on all streams instantiated in the system
    hStreamInfo.cmd = NT_INFO_CMD_READ_STREAM;
    if ((status = NT_InfoRead(hInfo, &hStreamInfo)) != NT_SUCCESS) {
        NT_ExplainError(status, errorBuffer, sizeof (errorBuffer) - 1);
        fprintf(stderr, "NT_InfoRead() failed: %s\n", errorBuffer);
        exit(0);
    }

    char pktCntStr[4096];
    memset(pktCntStr, 0, sizeof (pktCntStr));

    uint32_t stream_id = 0;
    uint32_t stream_cnt = 0;
    uint32_t numStreams = hStreamInfo.u.stream.data.count;

    for (stream_cnt = 0; stream_cnt < numStreams; ++stream_cnt) {

        do {
            // Read usage data for the chosen stream ID
            hStat.cmd = NT_STATISTICS_READ_CMD_USAGE_DATA_V0;
            hStat.u.usageData_v0.streamid = (uint8_t) stream_id;

            if ((status = NT_StatRead(hStatStream, &hStat)) != NT_SUCCESS) {
                // Get the status code as text
                NT_ExplainError(status, errorBuffer, sizeof (errorBuffer));
                fprintf(stderr, "NT_StatRead() failed: %s\n", errorBuffer);
                exit(0);
            }

            if (hStat.u.usageData_v0.data.numHostBufferUsed == 0) {
                streamStats[stream_id].active = false;
                ++stream_id;
                continue;
            }
        } while (hStat.u.usageData_v0.data.numHostBufferUsed == 0);

        streamStats[stream_id].active = true;
        streamStats[stream_id].aveOBFillLevel = 0;
        streamStats[stream_id].aveHBFillLevel = 0;

        for (hbCount = 0; hbCount < hStat.u.usageData_v0.data.numHostBufferUsed; hbCount++) {

        	// Get the adapter number for the host buffer.  Since a stream can be connected to multiple
        	// Hostbuffers/Adapters we build up a bit mask of the adapters this stream is using.
        	// We will decode this when we display the values.
        	//
            streamStats[stream_id].adapterNo |= (1 << hStat.u.usageData_v0.data.hb[hbCount].adapterNo);

            streamStats[stream_id].hb[hbCount].obFillLevel =
                    ((100 * hStat.u.usageData_v0.data.hb[hbCount].onboardBuffering.used) /
                    hStat.u.usageData_v0.data.hb[hbCount].onboardBuffering.size);

            if (streamStats[stream_id].hb[hbCount].obFillLevel > 100) {
                streamStats[stream_id].hb[hbCount].obFillLevel = 100;
            }

            uint32_t bufSize = hStat.u.usageData_v0.data.hb[hbCount].enQueuedAdapter / 1024
                    + hStat.u.usageData_v0.data.hb[hbCount].deQueued / 1024
                    + hStat.u.usageData_v0.data.hb[hbCount].enQueued / 1024
                    - HB_HIGHWATER;

            streamStats[stream_id].hb[hbCount].hbFillLevel = (uint32_t)
                    ((100 * hStat.u.usageData_v0.data.hb[hbCount].deQueued / 1024) /
                    bufSize);


            streamStats[stream_id].hb[hbCount].pktsPrev = streamStats[stream_id].hb[hbCount].pktsTotal;
            streamStats[stream_id].hb[hbCount].pktsTotal = hStat.u.usageData_v0.data.hb[hbCount].stat.rx.frames;

            streamStats[stream_id].pktsCurr = streamStats[stream_id].hb[hbCount].pktsTotal
                    - streamStats[stream_id].hb[hbCount].pktsPrev;


            streamStats[stream_id].hb[hbCount].dropPrev = streamStats[stream_id].hb[hbCount].dropTotal;
            streamStats[stream_id].hb[hbCount].dropTotal = hStat.u.usageData_v0.data.hb[hbCount].stat.drop.frames;

            streamStats[stream_id].dropCurr = streamStats[stream_id].hb[hbCount].dropTotal
                    - streamStats[stream_id].hb[hbCount].dropPrev;

            streamStats[stream_id].aveOBFillLevel += streamStats[stream_id].hb[hbCount].obFillLevel;
            streamStats[stream_id].aveHBFillLevel += streamStats[stream_id].hb[hbCount].hbFillLevel;
        }


        streamStats[stream_id].aveOBFillLevel /= hStat.u.usageData_v0.data.numHostBufferUsed;
        if (streamStats[stream_id].aveOBFillLevel > 100) streamStats[stream_id].aveOBFillLevel = 100;
        
        streamStats[stream_id].aveHBFillLevel /= hStat.u.usageData_v0.data.numHostBufferUsed;
        if (streamStats[stream_id].aveHBFillLevel > 100) streamStats[stream_id].aveHBFillLevel = 100;

        ++stream_id;
    }

    return numStreams;
}

double time_so_far(void) {
    struct timeval tp;

    if (gettimeofday(&tp, (struct timezone *) NULL) == -1)
        perror("gettimeofday");
    return ((double) (tp.tv_sec)) +
            (((double) tp.tv_usec) * 0.000001);
}

char * formatRate(uint64_t BytesPerSec, char *rateBuf) {
    if (8 * BytesPerSec / 1000 < 1000000) {
        sprintf(rateBuf, "%8.3f", (float) 8 * BytesPerSec / 1000000000);
    } else if (8 * BytesPerSec / 1000 < 10000000) {
        sprintf(rateBuf, "%8.2f", (float) 8 * BytesPerSec / 1000000000);
    } else {
        sprintf(rateBuf, "%8.1f", (float) 8 * BytesPerSec / 1000000000);
    }
    return rateBuf;
}

void outputStats(
        sysStats_t sysStats) {
    uint32_t i;

    char time[2048];

    //    printf("------------------------------------+\n");
    clear();
    int row = 1;
    int col = 0;

    mvprintw(row, 0, "%s", decodeTimeStamp(sysStats.time, time));
    row += 2;
    mvprintw(row, 0, "Adapter");
    mvprintw(row, 18, "PPS");
    mvprintw(row, 22, "Stream");
    mvprintw(row, 32, "Host Buffer Utilization");
    mvprintw(row, 68, "On-Board SDRAM Buffer Utilization");
    mvprintw(row, 108, "Drops/Sec");

    uint32_t cnt = 0;
    
    for (i = 0; i < sysStats.numStreams; ++i) {
        uint32_t val;
        row = i + 5;
        col = 0;
        
        while (!sysStats.streamStats[cnt].active && cnt < MAX_STREAMS) {
            ++cnt;
        }


        // Print out the adapter number(s).  This is passed in as a bitmask
        // so we are printing out the position of each bit set for the stream
        //
		uint32_t pcnt = 0;
        for (uint32_t acnt = 0; acnt < 4; ++acnt) {
        	if ( (1 << acnt & sysStats.streamStats[cnt].adapterNo) != 0) {
        		mvprintw(row, col, "%d ", acnt);
        		col +=2;
        	} else {
        		pcnt += 2;
        	}
        }
        col += 4 + pcnt;


        mvprintw(row, col, "%8d %4d:[", 2*sysStats.streamStats[cnt].pktsCurr, cnt);
        col += 15;
        for (val = 0; val < sysStats.streamStats[cnt].aveHBFillLevel; val += 3) {
            mvprintw(row, col++, "|");
        }
        for (val = sysStats.streamStats[cnt].aveHBFillLevel; val < 100; val += 3) {
            mvprintw(row, col++, " ");
        }
        mvprintw(row, col, "%3d%%]:[", sysStats.streamStats[cnt].aveHBFillLevel);
        col += 7;

        for (val = 0; val < sysStats.streamStats[cnt].aveOBFillLevel; val += 3) {
            mvprintw(row, col++, "|");
        }

        for (val = sysStats.streamStats[cnt].aveOBFillLevel; val < 100; val += 3) {
            mvprintw(row, col++, " ");
        }

        mvprintw(row, col++, "%4d%%] %8ld",
                sysStats.streamStats[cnt].aveOBFillLevel,
                sysStats.streamStats[cnt].dropCurr);
        ++cnt;
    }
/*
    row += 2;
    mvprintw(row, 1, "* indicates packets arriving on stream.");
*/

    refresh(); /* Print it on to the real screen */
}

int main(int argc, char**argv) {
    //int main() {
    //    FILE *f1;
    char *p = argv[0];
    int numOptions = 0;
    int option;
    int option_index = 0;
    sysStats_t sysStats;

    memset(&sysStats, 0, sizeof (sysStats));

    uint32_t i = 0;
    int status;
    char errorBuffer[80]; // Error buffer
    NtInfoStream_t hInfo;
    NtStatStream_t hStatStream;

    signal(SIGINT, StopApplication);

    //Initialize ncurces
    initscr(); /* Start curses mode 		  */
    start_color();
    use_default_colors();


    //-----    -----    -----    -----    -----    -----    -----    -----
    // Process command line options
    // Strip path
    for (; i != 0; i--) {
        if ((p[i - 1] == '.') || (p[i - 1] == '\\') || (p[i - 1] == '/')) {
            break;
        }
    }
    while ((option = getopt_long(argc, argv, "f:h", long_options, &option_index)) != -1) {
        numOptions++;
        switch (option) {
            case OPTION_HELP:
            case OPTION_SHORT_HELP:
                _Usage();
                break;

            default:
                printf("ERROR: Invalid command line option!!!");
                //invalidOption = 1;
                return -1;
                break;
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

    sysStats.numStreams = readStreamStats(sysStats.streamStats, hInfo, hStatStream);


    while (g_appRunning) {

        usleep(500000);
        struct timeval tv;
        gettimeofday(&tv, NULL);

        sysStats.time = ((tv.tv_sec * 1000000) + tv.tv_usec)*100;
        sysStats.numStreams = readStreamStats(sysStats.streamStats, hInfo, hStatStream);

        outputStats(sysStats);
    } // end while()

    //    backlog.write("Exit Loop!\n");

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
        struct timeval tv;
        gettimeofday(&tv, NULL);

    }

    endwin(); /* End curses mode		  */
}
