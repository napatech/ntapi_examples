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


#ifndef NTFILTERD_H
#define NTFILTERD_H

static const char usageText[] = 
"\n"
"This application will open a file that contains a list of IP addresses and set\n"
"NTPL filter expressions accordingly.  The program will monitor the file for\n"
"changes and update the filters as the file changes.  The entries in the file\n"
"will take one of the following forms:\n"
"    192.168.1.1         #address to match in either source or destination address fields.\n"
"    src dst 192.168.1.1 #same as above."
"    src 192.168.1.1     # address to match in only the source IP field."
"    dst 192.168.1.1     # address to match in only the destination IP field."
"    192.168.1.1/24      # filter subnet with 24 bit netmask"
"\n"
"    2000:0000:0000:0000:0000:0000:0000:0002  #IPv6 address."
"    2000::2  #short form of IPv6 Address"
"    "
"\n"
"\n"

"\n"
"\n"
  "Syntax:\n"
  "\n"
  "   ntfilterd -f <filename> [-s <stream(s)>] [-c <color>] [-p <priority>] [-d] [-k]\n"
  "\n"
  "commands:\n"
  "    --help, -h     : Displays this help and exit.\n"
  "\n"
  "    -f             : Name of file containing IP Addresses to filter.  If the\n"  
  "                     file already exists than the existing file will be \n"  
  "                     opened.  Otherwise, a new file will be created.  The \n"  
  "                     user can then add IP addresses to this newly created\n"
  "                     file."
  "\n"
  "     -l            : Name of file to which log messages are sent."
  "\n"
  "    -s             : The integer ID of the stream, or a range of streams \n"
  "                     (e.g. 0..3), to which filters will be applied.\n"  
  "\n"
  "    -c             : The integer ID of the color that is to be assigned to\n"
  "                     to the stream created.\n"
  "\n"
  "    -p             : The base priority from 0..61 that is to be applied to \n"
  "                     the filters created by this program.  This option allows\n"
  "                     the priority of filters created within the context of \n"
  "                     this program to be coordinated with filters created\n"
  "                     outside of this program. \n"
  "\n"
  "    -d             : Debug mode. In this mode the program will run within a\n"
  "                     terminal instead of as a daemon.\n\n"
  "\n"
#ifdef SUPPORT_KILL_ARG
  "    -k             : Terminate the running daemon.\n\n"  
  "\n"
#endif
;

// #define GREEN_BAY    


#endif /* NTFILTERD_H */

