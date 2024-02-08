
/*
 * Copyright 2023 Napatech A/S. All rights reserved.
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



// Include this in order to access the Napatech API


#include <nt.h>

#if defined(__linux__) || defined(__FreeBSD__)
  #include <unistd.h> // sleep()
#elif defined(WIN32) || defined (WIN64)
  #include <winsock2.h> // Sleep()
#endif

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

int main(void)

{
  NtError_t port_status;
  char errBuf[NT_ERRBUF_SIZE];
  //
  NtInfoStream_t hInfo;
  //
  NtInfo_t infoSystem;
  //
  unsigned int ports;
  unsigned port;
  //

  NtStatStream_t hStatStream; // Statistics stream handle
  NtStatistics_t hStat;       // Stat handle.
  char errorBuffer[NT_ERRBUF_SIZE];       // Error buffer
  int status;                 // Status variable


// Read number of ports from information stream

  // Initialize NTAPI library
  if ((status = NT_Init(NTAPI_VERSION)) != NT_SUCCESS) {
    NT_ExplainError(port_status, errBuf, sizeof(errBuf));
    fprintf(stderr, "ERROR: NT_Init failed. Code 0x%x = %s\n", port_status, errBuf);
    return port_status;
  }

  // Open the information stream
  if ((status = NT_InfoOpen(&hInfo, "bypass_info_example")) != NT_SUCCESS) {
    NT_ExplainError(port_status, errBuf, sizeof(errBuf));
    fprintf(stderr, ">>> Error: NT_InfoOpen failed. Code 0x%x = %s\n", port_status, errBuf);
    return port_status;
  }

  // Read system information
  infoSystem.cmd = NT_INFO_CMD_READ_SYSTEM;
  if ((status = NT_InfoRead(hInfo, &infoSystem)) != NT_SUCCESS) {
    NT_ExplainError(port_status, errBuf, sizeof(errBuf));
    fprintf(stderr, "ERROR: NT_InfoRead failed. Code 0x%x = %s\n", port_status, errBuf);
    return port_status;
  }

  printf("System: %d.%d.%d.%d\n\n", infoSystem.u.system.data.version.major,
                                  infoSystem.u.system.data.version.minor,
                                  infoSystem.u.system.data.version.patch,
                                  infoSystem.u.system.data.version.tag);
  printf("Ports: %u\n", infoSystem.u.system.data.numPorts);
  ports = infoSystem.u.system.data.numPorts;

   printf("\n");


// End read number of ports

  // Open the stat stream
  if ((status = NT_StatOpen(&hStatStream, "ExampleStat")) != NT_SUCCESS) {
    // Get the status code as text
    NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
    fprintf(stderr, "NT_StatOpen() failed: %s\n", errorBuffer);
    return -1;
  }

  // Read the statistics counters to clear the statistics
  // This is an optional step. If omitted, the adapter will show statistics form the start of ntservice.
  hStat.cmd=NT_STATISTICS_READ_CMD_QUERY_V3;
  hStat.u.query_v3.poll=0;  // Wait for a new set
  hStat.u.query_v3.clear=1; // Clear statistics
  if ((status = NT_StatRead(hStatStream, &hStat)) != NT_SUCCESS) {
    // Get the status code as text
    NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
    fprintf(stderr, "NT_StatRead() failed: %s\n", errorBuffer);
    return -1;
  }

  // Read new statistics for 10 seconds
  printf("Statistics for port 0 the next 10 seconds.\n");
  printf("--------------------------------------------------------------------------------\n");
  for (;;) {
    hStat.cmd=NT_STATISTICS_READ_CMD_QUERY_V3;
    hStat.u.query_v3.poll=1;  // The the current counters
    hStat.u.query_v3.clear=0; // Do not clear statistics
    if ((status = NT_StatRead(hStatStream, &hStat)) != NT_SUCCESS) {
      // Get the status code as text
      NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
      fprintf(stderr, "NT_StatRead() failed: %s\n", errorBuffer);
      return -1;
    } 

// Print the time and date

  printf("\n");

  time_t t = time(NULL);                                                        
  struct tm time = *localtime(&t);                                              
                                                                                
  printf("Date: %d:%d:%d\n", time.tm_mday,  time.tm_mon + 1, time.tm_year + 1900);
  printf("Time: %d:%d %d\n", time.tm_hour, time.tm_min, time.tm_sec);  
  printf("\n");

// End Print time and date

 for (port = 0; port < ports; port++){
    // Print the RMON1 pkts and octets counters Port 0
    for (port = 0; port < ports; port++)  {
      printf("Napatech Port %2d is OK - RX Packet Count: %016lld, Byte Count: %016lld\n",
             port, (unsigned long long) hStat.u.query_v3.data.port.aPorts[port].rx.RMON1.pkts, (unsigned long long) hStat.u.query_v3.data.port.aPorts[port].rx.RMON1.octets);
    }



    printf("--------------------------------------------------------------------------------\n");


    // Sleep 1 sec
#if defined(__linux__) || defined(__FreeBSD__)
    sleep(1);
#elif defined(WIN32) || defined (WIN64)
    Sleep(1000); // sleep 1000 milliseconds = 1 second
#endif
  }
}

  // Close the stat stream
  if ((status = NT_StatClose(hStatStream)) != NT_SUCCESS) {
    // Get the status code as text
    NT_ExplainError(status, errorBuffer, sizeof(errorBuffer));
    fprintf(stderr, "NT_StatClose() failed: %s\n", errorBuffer);
    return -1;
  }

  // Close down the NTAPI library
  NT_Done();

  printf("Done.\n");
  return 0;
}
