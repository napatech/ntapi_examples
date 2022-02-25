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


