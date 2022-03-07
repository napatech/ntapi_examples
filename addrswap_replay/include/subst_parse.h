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

/*
 * yaml_parse.h
 *
 *  Created on: Jan 29, 2021
 *      Author: napatech
 */

#ifndef INCLUDE_SUBST_PARSE_H_
#define INCLUDE_SUBST_PARSE_H_

#include <pcap.h>

struct pcap_desc {
	uint64_t ts;	/* time stamp */
	uint32_t caplen;	/* length of portion present */
	uint32_t len;	/* length this packet (off wire) */
};


/* The structures below has been implemented for Little endian format. */
struct MACHeader_s {
  uint8_t  mac_dest[6];
  uint8_t  mac_src[6];
  uint16_t mac_typelen;
#define  MAC_TYPE_IP4      0x0800
#define  MAC_TYPE_VLAN     0x8100
}; // 14 bytes
#define VLAN_TAG_LEN 4

struct IPv4Header_s {
  uint16_t ip_hl: 4;
  uint16_t ip_v: 4;
  uint16_t ip_tos: 8;
  uint16_t ip_len;

  uint32_t ip_id:16;
  uint32_t ip_frag_off:16;
#define IP_DONT_FRAGMENT  0x4000
#define IP_MORE_FRAGMENTS 0x2000

  uint32_t ip_ttl:8;
  uint32_t ip_prot:8;
#define IP_PROT_ICMP     1
#define IP_PROT_TCP      6
#define IP_PROT_UDP     17
#define IP_PROT_GRE     47
#define IP_PROT_ICMPv6  58
#define IP_PROT_SCTP   132
  uint32_t ip_crc:16;

  uint32_t ip_src;
  uint32_t ip_dst;
}; //20 bytes

struct UDPHeader_s {
  uint32_t udp_src:16;
  uint32_t udp_dest:16;

  uint32_t udp_len:16;
  uint32_t udp_crc:16;
}; // 8 bytes

struct TCPHeader_s {
  uint32_t tcp_src:16;
  uint32_t tcp_dest:16;

  uint32_t tcp_seq;
  uint32_t tcp_ack;

  uint32_t reserved:4;
  uint32_t tcp_doff:4;
  uint32_t tcp_ec_ctl:8;
  uint32_t tcp_window:16;

  uint32_t tcp_crc:16;
  uint32_t tcp_urgp:16;
}; // 20 bytes


struct pseudo_hdr_v4 {
  uint32_t ip_src;
  uint32_t ip_dst;
  uint8_t  zero;
  uint8_t  ip_prot;
  uint16_t len;
};

struct pseudo_hdr_v6 {
  uint8_t  ip_src[16];
  uint8_t  ip_dst[16];
  uint32_t len;
  uint8_t  zero[3];
  uint8_t  nxtHdr;
};






uint32_t load_substitute_addr_map(char * substitute_filename);
int search_addr_map(uint32_t val);
void delete_addr_map(void);

void PrintIP(uint32_t address);

#endif /* INCLUDE_SUBST_PARSE_H_ */
