/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   ntfilterd.h
 * Author: root
 *
 * Created on May 11, 2016, 2:58 PM
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

