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
