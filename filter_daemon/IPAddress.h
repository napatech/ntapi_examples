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


#ifndef IPADDRESS_H
#define IPADDRESS_H

#include <string>
#include <nt.h>

#include "ntfilterd.h"

using namespace std;

class IPAddress {
public:
    
    enum MatchKey {
        MatchKeyIPv4Src = 3, 
        MatchKeyIPv4Dst = 5,
        MatchKeyIPv6Src = 4, 
        MatchKeyIPv6Dst = 6
    };
    
    IPAddress();

    IPAddress(const IPAddress& orig) {
        *this = orig;
    };
    virtual ~IPAddress();

    bool verifyAddr(string IPAddr);
    bool parse(string IPFileLine);
    void display(void) const;
    bool addNTPL(void);
    bool removeNTPL(void) const;

    void operator=(const IPAddress &rhs) {
        m_src = rhs.m_src;
        m_dst = rhs.m_dst;
        m_addr = rhs.m_addr;
        m_ver = rhs.m_ver;
        m_ntplId = rhs.m_ntplId;
        m_netMask = rhs.m_netMask;
        m_rangeAddr = rhs.m_rangeAddr;
    }

    bool operator==(const IPAddress &other) const {
        string comp1 = m_addr + "-" + (m_src ? "1" : "0")+(m_dst ? "1" : "0")+m_netMask;
        string comp2 = other.m_addr + "-" + (other.m_src ? "1" : "0")+(other.m_dst ? "1" : "0")+other.m_netMask;
        return (comp1 == comp2);
        //return m_addr == other.m_addr;

    }

    bool operator!=(const IPAddress &other) const {
        return !(*this == other);
    }

    bool operator>(const IPAddress &other) const {
        string comp1 = m_addr + (m_src ? "1" : "0")+(m_dst ? "1" : "0")+m_netMask;
        string comp2 = other.m_addr + (other.m_src ? "1" : "0")+(other.m_dst ? "1" : "0")+other.m_netMask;
        return comp1 > comp2;
        //return m_addr > other.m_addr;
    }

    bool operator<(const IPAddress &other) const {
        string comp1 = m_addr + (m_src ? "1" : "0")+(m_dst ? "1" : "0")+m_netMask;
        string comp2 = other.m_addr + (other.m_src ? "1" : "0")+(other.m_dst ? "1" : "0")+other.m_netMask;
        return comp1 < comp2;
        //return m_addr < other.m_addr;
    }


private:

    enum IPVersion {
        UNKNOWN, IPv4, IPv6
    };


    bool m_src;
    bool m_dst;
    string m_addr;
    IPVersion m_ver;
    uint m_ntplId;
    string m_netMask; 
    string m_rangeAddr;
};

#endif /* IPADDRESS_H */

