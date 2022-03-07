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
 * %NT_SOFTWARE_LICENSE%
 */

/**
 * @file
 * Platform dependent defines and functions
 */

#ifndef __TOOL_DEF_H__
#define __TOOL_DEF_H__

//
//
//

//
// Compiler specific attributes
//
// TODO: move into separate file
//
#ifdef _MSC_VER
#define __attribute(a)
#define __attribute__(a)
#define INLINE __forceinline
#define NOUSE
#define __func__ __FUNCTION__
#else
#define INLINE inline
#define NOUSE __attribute__ ((unused))
#endif


//
// TODO: move into separate file
//
//#ifdef WIN32
#ifdef _MSC_VER
#ifndef likely
#define likely(x)    (x)
#endif
#ifndef unlikely
#define unlikely(x)  (x)
#endif
#else
#ifndef likely
#define likely(x)    __builtin_expect (!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x)  __builtin_expect (!!(x), 0)
#endif
#endif


//
//
//
#ifdef _MSC_VER

#include <process.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/timeb.h>

/************************************************************************/
/* Name decoration                                                      */
/************************************************************************/
#define NAMED(name) _##name

/************************************************************************/
/* Thread handling                                                      */
/************************************************************************/
#define THREAD_NULL 0
#define THREAD_API unsigned __stdcall
#define THREAD_RETURN_OK { _endthreadex(0); return 0; }
#define THREAD_RETURN_ERROR(a) { _endthreadex(0); return a; }

typedef unsigned (__stdcall *ThreadStartAddress_t)(void *parameter);
typedef HANDLE ThreadHandle_t;

static INLINE int tools_thread_create(ThreadHandle_t* thread, ThreadStartAddress_t start_routine, void *parameter)
{
  *thread = (HANDLE)_beginthreadex( NULL, 0, start_routine, parameter, 0, NULL);
  return (*thread ? 0 : -1);
}

static INLINE int tools_thread_join(ThreadHandle_t thread)
{
  WaitForSingleObject(thread, INFINITE);
  CloseHandle(thread);
  return 0;
}

/************************************************************************/
/* time   handling                                                      */
/************************************************************************/
static INLINE void tools_sleep(int time)
{
  Sleep(time * 1000); /* From seconds to milliseconds */
}
static INLINE void tools_usleep(unsigned long usec)
{
  assert(usec > 1000);
  Sleep(usec / 1000); /* From useconds to milliseconds */
}

static INLINE void tools_gettimeofday(struct timeval *tv)
{
  struct __timeb64 timebuffer;
  _ftime64_s(&timebuffer);
  tv->tv_sec = (long)timebuffer.time;
  tv->tv_usec = timebuffer.millitm * 1000;
}

#define strcasecmp  _stricmp
#define strncasecmp _strnicmp
#define strtoll     _strtoi64
#define strtoull    _strtoui64

#define strdup      _strdup

//#include <io.h>
#define open        _open
//#define lseek       _lseek
#define close       _close
#define read        _read
#define write       _write
#define off_t       unsigned __int64  // UNIX: off_t = 64-uint, but in Windows types.h: off_t = long (32-int)
#define useconds_t  unsigned int

#define STRING2(x) #x
#define STRING(x) STRING2(x)

/************************************************************************/
/* File function                                                        */
/************************************************************************/
HANDLE fileopen(char *lpFileName, DWORD dwDesiredAccess, DWORD dwCreationDisposition);
BOOL filewrite(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite);
BOOL fileclose(HANDLE hObject);
DWORD filelseek(HANDLE hFile, LONG lDistanceToMove, int origin );

/************************************************************************/
/* Console ctrl handler definitions                                     */
/************************************************************************/
#define HANDLER_API         BOOL WINAPI
#define HANDLER_RETURN_OK   return true;
#define HANDLER_RETURN_ERR  return false;

#define CTR_SET_BEGIN(func, flag) \
  if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)func, flag)) {
#define CTR_SET_END \
  }


#else  // _WIN32


#include <stdint.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

/************************************************************************/
/* Console ctrl handler definitions                                     */
/************************************************************************/
#define HANDLER_API         void
#define HANDLER_RETURN_OK
#define HANDLER_RETURN_ERR

#define CTR_SIG_BEGIN(sig, func) \
{ \
  struct sigaction newaction; \
  memset(&newaction, 0, sizeof(newaction)); \
  newaction.sa_handler = func; \
  if (sigaction(sig, &newaction, NULL) < 0) {
#define CTR_SIG_END \
  } \
}

/************************************************************************/
/* Name decoration                                                      */
/************************************************************************/
#define NAMED(name) name

/************************************************************************/
/* Thread handling                                                      */
/************************************************************************/

#define THREAD_NULL NULL // NULL is ((void *)0)
#define THREAD_API void*
#define THREAD_RETURN_OK return NULL;
#define THREAD_RETURN_ERROR(a) return ((void*)(intptr_t)a);

typedef void* (*ThreadStartAddress_t)(void *parameter);
typedef pthread_t ThreadHandle_t;

static INLINE int tools_thread_create(ThreadHandle_t* thread, ThreadStartAddress_t start_routine, void *parameter)
{
  return pthread_create(thread, NULL, start_routine, parameter);
}
static INLINE int tools_thread_join(ThreadHandle_t thread)
{
  return pthread_join(thread, NULL);
}

/************************************************************************/
/* time   handling                                                      */
/************************************************************************/
static INLINE void tools_sleep(int time)
{
  sleep(time);
}
static INLINE void tools_usleep(useconds_t usec)
{
  usleep(usec);
}

static INLINE int tools_gettimeofday(struct timeval *tv)
{
  return gettimeofday(tv, NULL);
}

#endif // _WIN32

#endif // __TOOL_DEF_H__

//
// EOF
//
