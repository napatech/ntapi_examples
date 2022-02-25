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
 
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <linux/inotify.h>

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

int main( )
{
  int length, i = 0;
  int fd;
  int wd;
  char buffer[EVENT_BUF_LEN];

  //creating the INOTIFY instance
  fd = inotify_init();

  //checking for error
  if ( fd < 0 ) {
    perror( "inotify_init" );
  }

  //adding the “/tmp” directory into watch list. Here, the suggestion is to validate the existence of the directory before adding into monitoring list.
  wd = inotify_add_watch( fd, "/tmp", IN_CREATE | IN_DELETE );

  //read to determine the event change happens on “/tmp” directory. Actually this read blocks until the change event occurs

  length = read( fd, buffer, EVENT_BUF_LEN ); 

  //checking for error
  if ( length < 0 ) {
    perror( "read" );
  }  

  //actually read return the list of change events happens. Here, read the change event one by one and process it accordingly.
  while ( i < length ) {     struct inotify_event *event = ( struct inotify_event * ) &buffer[ i ];     if ( event->len ) {
      if ( event->mask & IN_CREATE ) {
        if ( event->mask & IN_ISDIR ) {
          printf( "New directory %s created.\n", event->name );
        }
        else {
          printf( "New file %s created.\n", event->name );
        }
      }
      else if ( event->mask & IN_DELETE ) {
        if ( event->mask & IN_ISDIR ) {
          printf( "Directory %s deleted.\n", event->name );
        }
        else {
          printf( "File %s deleted.\n", event->name );
        }
      }
    }
    i += EVENT_SIZE + event->len;
  }
  //removing the “/tmp” directory from the watch list.
   inotify_rm_watch( fd, wd );

  /*closing the INOTIFY instance*/
   close( fd );

}


*/



 #include <sys/inotify.h>
 #include <limits.h>
 #include "tlpi_hdr.h"
 
 static void             /* Display information from inotify_event structure */
 displayInotifyEvent(struct inotify_event *i)
 {
     printf("    wd =%2d; ", i->wd);
     if (i->cookie > 0)
         printf("cookie =%4d; ", i->cookie);
 
     printf("mask = ");
     if (i->mask & IN_ACCESS)        printf("IN_ACCESS ");
     if (i->mask & IN_ATTRIB)        printf("IN_ATTRIB ");
     if (i->mask & IN_CLOSE_NOWRITE) printf("IN_CLOSE_NOWRITE ");
     if (i->mask & IN_CLOSE_WRITE)   printf("IN_CLOSE_WRITE ");
     if (i->mask & IN_CREATE)        printf("IN_CREATE ");
     if (i->mask & IN_DELETE)        printf("IN_DELETE ");
     if (i->mask & IN_DELETE_SELF)   printf("IN_DELETE_SELF ");
     if (i->mask & IN_IGNORED)       printf("IN_IGNORED ");
     if (i->mask & IN_ISDIR)         printf("IN_ISDIR ");
     if (i->mask & IN_MODIFY)        printf("IN_MODIFY ");
     if (i->mask & IN_MOVE_SELF)     printf("IN_MOVE_SELF ");
     if (i->mask & IN_MOVED_FROM)    printf("IN_MOVED_FROM ");
     if (i->mask & IN_MOVED_TO)      printf("IN_MOVED_TO ");
     if (i->mask & IN_OPEN)          printf("IN_OPEN ");
     if (i->mask & IN_Q_OVERFLOW)    printf("IN_Q_OVERFLOW ");
     if (i->mask & IN_UNMOUNT)       printf("IN_UNMOUNT ");
     printf("\n");
 
     if (i->len > 0)
         printf("        name = %s\n", i->name);
 }
 
 #define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))
 
 int
 main(int argc, char *argv[])
 {
     int inotifyFd, wd, j;
     char buf[BUF_LEN] __attribute__ ((aligned(8)));
     ssize_t numRead;
     char *p;
     struct inotify_event *event;
 
     if (argc < 2 || strcmp(argv[1], "--help") == 0)
         usageErr("%s pathname...\n", argv[0]);
 
     inotifyFd = inotify_init();                 /* Create inotify instance */
     if (inotifyFd == -1)
         errExit("inotify_init");
 
+    /* For each command-line argument, add a watch for all events */
+
     for (j = 1; j < argc; j++) {
         wd = inotify_add_watch(inotifyFd, argv[j], IN_ALL_EVENTS);
         if (wd == -1)
             errExit("inotify_add_watch");
 
         printf("Watching %s using wd %d\n", argv[j], wd);
     }
 
     for (;;) {                                  /* Read events forever */
         numRead = read(inotifyFd, buf, BUF_LEN);
         if (numRead == 0)
             fatal("read() from inotify fd returned 0!");
 
         if (numRead == -1)
             errExit("read");
 
         printf("Read %ld bytes from inotify fd\n", (long) numRead);
 
         /* Process all of the events in buffer returned by read() */
 
         for (p = buf; p < buf + numRead; ) {
             event = (struct inotify_event *) p;
             displayInotifyEvent(event);
 
             p += sizeof(struct inotify_event) + event->len;
         }
     }
 
     exit(EXIT_SUCCESS);
 }
