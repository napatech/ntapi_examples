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

//
// tool_common.h
//

/*
 * Copyright 2019 Napatech A/S. All rights reserved.
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
#ifndef __TOOL_COMMON_H__
#define __TOOL_COMMON_H__

//Recommended error codes returned by tools and applications
//ErrorCodes > NT_APPL_ERROR_LAST are application specific

// NOTE: Do only added codes to the end if the enum in order to 
// keep backward compatibility.

typedef enum {
  NT_APPL_SUCCESS            = 0, //No error

  NT_APPL_ERROR_OPT_ILL      = 1, //An illegal option in the command line

  NT_APPL_ERROR_OPT_ARG      = 2, //An option argument was missing, invalid or
                                  //out of range

  NT_APPL_ERROR_OPT_CONFLICT = 3, //Conflict between options or option values in
                                  //the command line.
                                  //E.g. mutually exclusive options or setting a
                                  //low value higher than a high value.

  NT_APPL_ERROR_OPT_MISSING  = 4, //Missing option(s) in the command line

  NT_APPL_ERROR_CONFIG       = 5, //There is a conflict in the configuration
                                  //understood in a broad sense (setup, HW, 
                                  //libraries etc) or an option is in conflict with
                                  //the configuration.
                                  //E.g. when specifying an adapter that is not present.

  NT_APPL_ERROR_NTAPI        = 6, //A Napatech API function returned an error

  NT_APPL_ERROR_OS           = 7, //An OS API or other library function returned an
                                  //error

  NT_APPL_ERROR_INTERNAL     = 8, //An internal test has failed (Code error).
                                  //E.g. missing case in switch etc

  NT_APPL_ERROR_INPUT        = 9, //The application terminated due to input or
                                  //registered error in input:
                                  //E.g. traffic, link status, file content etc

  NT_APPL_CANCEL             = 10, //The user has cancelled the app.
  NT_APPL_ERROR_TIMEOUT      = 11, // Timeout

  NT_APPL_ERROR_LAST               // Placeholder - Not to be returned but can be used internally
} NtApplExitCode_t;

#endif // __TOOL_COMMON_H__

//
// EOF
//
