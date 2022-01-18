
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>    


#include <ctime>
#include <algorithm>


#include <syslog.h>
#include "NTFileHandler.h"
#include "IPAddress.h"
#include "NTUtils.h"

NTFileHandler::NTFileHandler()
{
}

//NTFileHandler::NTFileHandler(const NTFileHandler& orig) {
//}

NTFileHandler::~NTFileHandler() {
}

bool NTFileHandler::open(string filterFile) {
    bool retval = false;

    m_filterFile = filterFile;

    m_ntfilter.open(m_filterFile.c_str(), ios::in);
    if (m_ntfilter.is_open()) {
        retval = true;
        NTUtils::log(LOG_INFO, "Opening existing file %s", m_filterFile.c_str());
    } else {
        NTUtils::log(LOG_INFO, "Creating new file %s", m_filterFile.c_str());
        m_ntfilter.open(m_filterFile.c_str(), ios::out);
        if (!m_ntfilter.is_open()) {
            NTUtils::log(LOG_ERR, "Failed to create file %s! (Invalid path perhaps?)", m_filterFile.c_str());
        }
    }

    return retval;
}


void NTFileHandler::close(void) {
    set<IPAddress>::iterator it;
    
    for (it = m_appliedAddrs.begin(); it != m_appliedAddrs.end(); ++it) {
        (*it).removeNTPL();
    }
    m_ntfilter.close();
}

bool NTFileHandler::readFile(void) {

    uint count = 0;
    uint lineNo = 0;
    ifstream input(m_filterFile.c_str());

    m_readAddrs.clear();

    for (string filterLine; getline(input, filterLine);) {
        ++lineNo;
        IPAddress addr;

        // Remove any comments that are found.
        string::size_type i;
        if ((i = filterLine.find("#")) != string::npos) {
            filterLine.erase(i, filterLine.length() + 1);
        }        
        
        if (filterLine.length() > 0) {
            ++count;

            if (addr.parse(filterLine)) {
                addr.display();
                m_readAddrs.insert(addr);
            } else {
                NTUtils::log(LOG_ERR, "    Error at line %d: %s", lineNo, filterLine.c_str());
            }
        }
    }
    return count;
}

bool NTFileHandler::watchFile(bool *applicationRunning) {
    bool retVal = false;
    struct stat buffer;

    while (*applicationRunning) {
        int result = stat(m_filterFile.c_str(), &buffer);
        if (result != 0) {
            NTUtils::log(LOG_ERR, "stat of file %s failed!", m_filterFile.c_str());
        }

        char *datetime = (char *)malloc(100);
        memset(datetime, 0, 100);
        struct tm* time = localtime(&buffer.st_mtime);

        if ((time->tm_sec != m_prevTime.tm_sec) ||
                (time->tm_min != m_prevTime.tm_min) ||
                (time->tm_hour != m_prevTime.tm_hour) ||
                (time->tm_yday != m_prevTime.tm_yday) ||
                (time->tm_year != m_prevTime.tm_year)) {
            
            m_prevTime = *time;
            
//            NTUtils::log(LOG_NOTICE, "FilterFile %s has changed.", m_filterFile.c_str());
            retVal = true;
            break;
        }
        result = strftime(datetime, 100, "%c", time);
        free(datetime);
        sleep(1);
    }

    return retVal;
}

bool NTFileHandler::process(void) {

    set<IPAddress>::iterator it;
    set<IPAddress> toAdd;
    set<IPAddress> toRemove;
    set<IPAddress> toKeep;

    NTUtils::log(LOG_INFO, "------------------------------------------------");
    NTUtils::log(LOG_INFO, "Processing Filter File %s", m_filterFile.c_str());

    readFile();

    NTUtils::log(LOG_INFO, "Adding:");
    set_difference(m_readAddrs.begin(), m_readAddrs.end(),
            m_appliedAddrs.begin(), m_appliedAddrs.end(),
            inserter(toAdd, toAdd.begin())
            );
    for (it = toAdd.begin(); it != toAdd.end(); ++it) {
        (*it).display();
    }

    NTUtils::log(LOG_INFO, "Removing:");
    set_difference(m_appliedAddrs.begin(), m_appliedAddrs.end(),
            m_readAddrs.begin(), m_readAddrs.end(),
            inserter(toRemove, toRemove.begin())
            );
    for (it = toRemove.begin(); it != toRemove.end(); ++it) {
        (*it).display(); 
    }

    NTUtils::log(LOG_INFO, "Keeping:");
    set_difference(m_appliedAddrs.begin(), m_appliedAddrs.end(),
            toRemove.begin(), toRemove.end(),
            inserter(toKeep, toKeep.begin())
            );
    for (it = toKeep.begin(); it != toKeep.end(); ++it) {
        (*it).display();
    }

    NTUtils::log(LOG_INFO, "NTPL:");
    
    for (it = toRemove.begin(); it != toRemove.end(); ++it) {
        (*it).removeNTPL();
    }
   
    for (it = toAdd.begin(); it != toAdd.end(); ++it) {
        IPAddress *ipAddr = (IPAddress *)&(*it);
        if (!ipAddr->addNTPL()) {
            toAdd.erase(it);
        }
    }
    
    m_appliedAddrs = toKeep;
    set_union(m_appliedAddrs.begin(), m_appliedAddrs.end(),
            toAdd.begin(), toAdd.end(),
            inserter(m_appliedAddrs, m_appliedAddrs.begin())
            );

    NTUtils::log(LOG_INFO, "Currently Applied:");
    for (it = m_appliedAddrs.begin(); it != m_appliedAddrs.end(); ++it) {
        (*it).display();
    }

    return true;
}
