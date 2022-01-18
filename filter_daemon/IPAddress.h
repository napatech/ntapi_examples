/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   IPAddress.h
 * Author: root
 *
 * Created on May 17, 2016, 2:49 PM
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

