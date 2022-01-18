/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   IPAddress.cpp
 * Author: root
 * 
 * Created on May 17, 2016, 2:49 PM
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>   
#include <syslog.h>



#include <algorithm>

#include <nt.h>

#include "IPAddress.h"
#include "NTUtils.h"

IPAddress::IPAddress()
: m_src(false),
m_dst(false),
m_addr(""),
m_ver(UNKNOWN),
m_ntplId(0),
m_netMask("") {
}

//IPAddress::IPAddress(const IPAddress& orig) {
//}

IPAddress::~IPAddress() {
}

void IPAddress::display(void) const {

    if (m_netMask != "") {
        NTUtils::log(LOG_INFO, "    %s%s: %s %s - netmask: %s",
                (m_src == true ? "src " : " -  "),
                (m_dst == true ? "dst " : " -  "),
                (m_ver == IPv4 ? "IPv4" : (m_ver == IPv6 ? "IPv6" : "UNKNOWN")),
                m_addr.c_str(),
                m_netMask.c_str()
                );
    } else if (m_rangeAddr != "") {
        NTUtils::log(LOG_INFO, "    %s%s: %s %s..%s",
                (m_src == true ? "src " : " -  "),
                (m_dst == true ? "dst " : " -  "),
                (m_ver == IPv4 ? "IPv4" : (m_ver == IPv6 ? "IPv6" : "UNKNOWN")),
                m_addr.c_str(),
                m_rangeAddr.c_str()
                );
    } else {
        NTUtils::log(LOG_INFO, "    %s%s: %s %s",
                (m_src == true ? "src " : " -  "),
                (m_dst == true ? "dst " : " -  "),
                (m_ver == IPv4 ? "IPv4" : (m_ver == IPv6 ? "IPv6" : "UNKNOWN")),
                m_addr.c_str()
                );
    }
}

bool IPAddress::verifyAddr(string ipAddr) {

    // To do:
    //     Actually check the string to make sure it is a valid IPv4 or IPv6
    //     address.  For now just make sure it's non-zero length.
    //

    // This is crude but works for now...
    if ((ipAddr.find(".")) != string::npos) {
        m_ver = IPv4;
    } else if ((ipAddr.find(":")) != string::npos) {
        m_ver = IPv6;
    }

    return ipAddr.length() > 0;
}

bool IPAddress::addNTPL(void) {

    char *ntplAddr;
    if (m_netMask != "") {
        asprintf(&ntplAddr, "{[%s]:[%s]}", m_netMask.c_str(), m_addr.c_str());
    } else if (m_rangeAddr != "") {
        asprintf(&ntplAddr, "([%s]..[%s])", m_addr.c_str(), m_rangeAddr.c_str());

    } else {
        asprintf(&ntplAddr, "[%s]", m_addr.c_str());
    }

    switch (m_ver) {
        case IPv4:
            if (m_src && m_dst) {
                //                m_ntplId = NTUtils::NTPL((const char *) "IPMatchList[KeySet=1] = IPv4Addr == %s", ntplAddr);
                m_ntplId = NTUtils::NTPL((const char *) "IPMatchList = IPv4Addr == %s", ntplAddr);
            } else if (m_src) {
                m_ntplId = NTUtils::NTPL((const char *) "IPMatchList[KeySet=%d] = IPv4Addr == %s",
                        IPAddress::MatchKeyIPv4Src, ntplAddr);
            } else if (m_dst) {
                m_ntplId = NTUtils::NTPL((const char *) "IPMatchList[KeySet=%d] = IPv4Addr == %s",
                        IPAddress::MatchKeyIPv4Dst, ntplAddr);
            }
            break;
        case IPv6:
            if (m_src && m_dst) {
                m_ntplId = NTUtils::NTPL((const char *) "IPMatchList = IPv6Addr == %s", ntplAddr);
            } else if (m_src) {
                m_ntplId = NTUtils::NTPL((const char *) "IPMatchList[KeySet=%d] = IPv6Addr  == %s",
                        IPAddress::MatchKeyIPv6Src, ntplAddr);
            } else if (m_dst) {
                m_ntplId = NTUtils::NTPL((const char *) "IPMatchList[KeySet=%d] = IPv6Addr == %s",
                        IPAddress::MatchKeyIPv6Dst, ntplAddr);
            }
            break;
        case UNKNOWN:
        default:
            m_ntplId = 0;
            break;
    }
    free(ntplAddr);

    return (m_ntplId > 0);
}

bool IPAddress::removeNTPL(void) const {
    //    NTUtils::log(LOG_NOTICE, "removing IPMatch filter for %s %s,%s", m_addr.c_str(),
    //            m_src ? "src" : "", m_dst ? "dst" : "");

    if (m_ntplId > 0) {
        NTUtils::NTPL((const char *) "delete = %d", m_ntplId);
    }

    return true;
}

bool IPAddress::parse(string s) {
    string::size_type i;

    transform(s.begin(), s.end(), s.begin(), ::tolower);

    string src = "src";
    if ((i = s.find(src)) != string::npos) {
        m_src = true;
        s.erase(i, src.length() + 1);
    }

    string dst = "dst";
    if ((i = s.find(dst)) != string::npos) {
        m_dst = true;
        s.erase(i, dst.length() + 1);
    }

    if ((m_src == false) && (m_dst == false)) {
        // Neither is specified.  Default is both.
        m_src = true;
        m_dst = true;
    }

    // Remove white space
    string space = " ";
    while ((i = s.find(space)) != string::npos) {
        s.erase(i, space.length());
    }

    string tab = "\t";
    while ((i = s.find(tab)) != string::npos) {
        s.erase(i, tab.length());
    }

    if ((s.find(".")) != string::npos) {
        m_ver = IPv4;
    } else if ((s.find(":")) != string::npos) {
        m_ver = IPv6;
    }

    // Find subnet bits if any
    string slash = "/";
    if ((i = s.find(slash)) != string::npos) {
        uint subnetBits = strtoul(s.substr(i + 1, s.length()).c_str(), NULL, 0);
        short subnet[16] = {0};
        uint pos = 0;

        for (uint bit = 0; bit < subnetBits; ++bit) {

            subnet[pos] |= 1 << (7 - (bit % 8));

            if ((bit % 8) == 7) {
                ++pos;
            }
        }
        if (m_ver == IPv4) {
            char *buf;
            asprintf(&buf, "%02x.%02x.%02x.%02x",
                    subnet[0] & 0xff,
                    subnet[1] & 0xff,
                    subnet[2] & 0xff,
                    subnet[3] & 0xff
                    );
            m_netMask = buf;
           free(buf);

           

        } else if (m_ver == IPv6) {
            char *buf;
            asprintf(&buf, "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
                    subnet[0] & 0xff,
                    subnet[1] & 0xff,
                    subnet[2] & 0xff,
                    subnet[3] & 0xff,
                    subnet[4] & 0xff,
                    subnet[5] & 0xff,
                    subnet[6] & 0xff,
                    subnet[7] & 0xff,
                    subnet[8] & 0xff,
                    subnet[9] & 0xff,
                    subnet[10] & 0xff,
                    subnet[11] & 0xff,
                    subnet[12] & 0xff,
                    subnet[13] & 0xff,
                    subnet[14] & 0xff,
                    subnet[15] & 0xff
                    );
            m_netMask = buf;
            free(buf);

        }
        s.erase(i, s.length());
    }

    string doubleDot = "..";
    if ((i = s.find(doubleDot)) != string::npos) {
        m_rangeAddr = s.substr(i + 2, s.length());
        s.erase(i, s.length());
    }

    m_addr = s;

    return verifyAddr(m_addr);
}


