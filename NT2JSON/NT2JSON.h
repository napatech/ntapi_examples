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


#ifndef __PORTSTATS_H__
#define __PORTSTATS_H__


static const char usageText[] =
  "The NT2JSON program reads product, sensor, statistics, etc information and \n"
  "outputs the information in JSON format  .\n\n"
  "Syntax:\n"
  "  NT2JSON [--help] [-h] [--system] [--product] [--timesync] [--sensors] [--alarm] [--port] [--stream]\n"
  "\n"
  "Required Arguments:\n"
  "  \n"
  "\nOptional Arguments:\n"
  "  -f <filename>       : Name of json file to output.  If not specified the output\n"
  "                        will be displayed in the terminal.\n"
  "  -p                  : Write the file in pretty-print format.\n"
  "\n"
  "Output Options:"
  "    If no output options are specified then all options will be output\n"
  "\n"
  "    --system          : Output system information.\n\n"
  "    --product         : Output the Product info.\n\n"
  "    --timesync        : Output TimeSync information.\n\n"
  "    --sensors         : Output Sensor information - Cannot be combined with alarm\n"
  "                        option.\n\n"
  "    --alarm           : Output only Sensor information that is in an alarm state\n"
  "                        cannot be combined with sensors option.\n\n"
  "    --port            : Output Port information and statistics.\n\n"
  "    --rxport          : Output Rx Port information and statistics.\n\n"
  "    --txport          : Output Tx Port information and statistics.\n\n"
  "    --portsummary      : Output simplified version of port statistics.\n\n"
  "    --stream          : Output Stream information and statistics.\n\n"
  "    --color           : Output the color counters for each adapter.\n\n"
  "    --mergedcolor     : Sums and displays the corresponding color counters \n"
  "                        from multiple adapters.  I.e. The output value for\n"
  "                        counter X is the sum of counters X1+X2...+Xn from  \n"
  "                        each of n adapters.  This is useful when a single\n"
  "                        filter applies to multiple adapter.\n\n"
  "    --help, -h        : Displays the help.\n";
        

#endif /* __PORTSTATS_H__ */


