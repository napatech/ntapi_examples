/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   backlog.cpp
 * Author: root
 * 
 * Created on March 8, 2017, 3:21 PM
 */

#include <stdio.h>
#include "backlog.h"

#define DELIMITER    "------------------------------------------------------------------------------------------------------\n"
#define ENDDELIMITER "======================================================================================================\n"

CBacklog::CBacklog() : m_currPos(0) {
    for (int i = 0; i < NUM_ENTRIES; ++i) {
        m_processed[i] = true;
    }
}

//CBacklog::CBacklog(const CBacklog& orig) {
//}

CBacklog::~CBacklog() {
}

bool CBacklog::initialize(string filename, string timeStr) {

    if (filename.length() > 0) {
        printf("\nWriting output to file: %s\n", filename.c_str());
        m_file = fopen(filename.c_str(), "w");
    } else {
        m_file = NULL;
    }
    printf("\nWriting output to stdout: %s\n", filename.c_str());

    if (m_file == NULL) {
        fprintf(stderr, "Failed to open %s\n", filename.c_str());
        return false;
    }
    fprintf(m_file, "%s\nFile created at: %s\n%s",
            ENDDELIMITER,
            timeStr.c_str(),
            DELIMITER);
    fflush(m_file);


    return true;
}

void CBacklog::shutdown(string timeStr) {
    if (m_file) {
        fprintf(m_file, "%s\nFile closed at: %s\n%s",
                ENDDELIMITER,
                timeStr.c_str(),
                ENDDELIMITER);
        fclose(m_file);
    }
}

void CBacklog::write(string str) {
    if (m_file)
        fprintf(m_file, "%s", str.c_str());
};

void CBacklog::output(bool endDelimiter) {

    unsigned int currPos = m_currPos;
    for (unsigned int i = 0; i <= NUM_ENTRIES; ++i) {
        if (m_processed[currPos] == false) {
            m_processed[currPos] = true;
            if (m_file) {
                fprintf(m_file, DELIMITER);
                fprintf(m_file, "time: %s\n", m_time[currPos].c_str());
                //            fprintf(m_file, "\nnumStreams: %s\n", m_numStreams[currPos].c_str());

                fprintf(m_file, "-------- Ports --------\n");
                fprintf(m_file, "Mbps: \n%s\n", m_rxMbpsStr[currPos].c_str());
                fprintf(m_file, "\n-------- Streams --------\n");
                fprintf(m_file, "pkts: \n%s\n", m_pktsStr[currPos].c_str());
                fprintf(m_file, "\nGbps: \n%s\n", m_pktsRateStr[currPos].c_str());
                fprintf(m_file, "\nhbuffUtil: \n%s\n", m_hbFillStr[currPos].c_str());
                fprintf(m_file, "\nsdramUtil: \n%s\n", m_obFillStr[currPos].c_str());
                fprintf(m_file, "\npktsDrop : \n%s\n", m_dropStr[currPos].c_str());
                fprintf(m_file, "\n-------- CPU --------\n");
                fprintf(m_file, "Utilization:\n%s\n", m_utilString[currPos].c_str());
            }

            FILE *myStdout = stdout;
            fprintf(myStdout, DELIMITER);
            fprintf(myStdout, "time: %s\n", m_time[currPos].c_str());
            //            fprintf(m_file, "\nnumStreams: %s\n", m_numStreams[currPos].c_str());

            fprintf(myStdout, "-------- Ports --------\n");
            fprintf(myStdout, "Mbps: \n%s\n", m_rxMbpsStr[currPos].c_str());
            fprintf(myStdout, "\n-------- Streams --------\n");
            fprintf(myStdout, "pkts: \n%s\n", m_pktsStr[currPos].c_str());
            fprintf(myStdout, "\nGbps: \n%s\n", m_pktsRateStr[currPos].c_str());
            fprintf(myStdout, "\nhbuffUtil: \n%s\n", m_hbFillStr[currPos].c_str());
            fprintf(myStdout, "\nsdramUtil: \n%s\n", m_obFillStr[currPos].c_str());
            fprintf(myStdout, "\npktsDrop : \n%s\n", m_dropStr[currPos].c_str());
            fprintf(myStdout, "\n-------- CPU --------\n");
            fprintf(myStdout, "Utilization:\n%s\n", m_utilString[currPos].c_str());
        }

        currPos = (currPos + 1) % NUM_ENTRIES;
    }
    if (endDelimiter) {
        fprintf(m_file, ENDDELIMITER);
    }
    fflush(m_file);
}
