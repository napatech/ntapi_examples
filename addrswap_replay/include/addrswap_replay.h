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

/**
 * @file
 *
 * This header file contains documentation on the replay tool.
 */
#ifndef __REPLAY_H__
#define __REPLAY_H__

static const char *usageText[] = {
  "The addrswap_replay tool can replay a PCAP data file out of a specified port\n"
  "with IP address fields optionally replaced with values provided via a substitution file.\n\n"
  "The substitution file contains a list of IP addresses found in the file followed by the\n"
  "address to replace it with when transmitting the packet.  E.g.:\n"
		"    10.0.2.15: 1.1.1.1\n"
		"    10.0.2.16: 1.1.1.2\n"
		"    10.0.2.17: 1.1.1.3\n"
		"    10.0.2.18: 1.1.1.4\n\n"
  "Note that there must not be any white space preceding each line\n"
  "and there must be a space after each ':'.\n\n"

  "The replay rate will be identical to the rate at which the packets were received.\n"
  "\n"
  "Note: this program requires that an FPGA image containing the Napatech Test & Measurement Feature Set.\n\n"
  "Syntax:\n"
  "  addrswap_replay -f <file> -p <port> [-s <substitution_file>] [-n <numa_node>] [--help] \n",
  "\nExamples:\n\n"
  "    # addrswap_replay -p -f file.pcap -s file.yaml\n"
  "        replay a file out port 1 substituting addresses in the PCAP file with addresses replacements\n"
  "        provided in the substitution file.\n",
  NULL};

#endif
