
#ifndef __BUFFERPROFILE_H__
#define __BUFFERPROFILE_H__

static const char usageText[] =
  "\nThis program will track port, buffer and CPU utilization and monitor for\n"
  "packet loss.  When dropped packets are detected information about the state\n"
  "of the system is output every second; starting five seconds prior to the drop\n"
  "and continuing until no more drops are detected.\n"
  "\n"
  "Syntax:\n"
  "  bufferProfile [--help]  [-h] [-f] [-d] [-k] [-c]\n"
  "\n"
  "\nOptional Arguments:\n"
  "\n  -f <filename>   : Name of file where update will be output.  This is a \n"
  "                    : required option when running as a daemon\n"
  "\n  -c              : Continuous operation.  This option will output statistics\n"
  "                    : every second regardless of whether drops have occured.\n"
  "\n  -d              : Run this program as a daemon.  The -f <filename> option\n"
  "                    : must be specified when running as a daemon\n"
  "\n  -k              : Kill a running daemon instance.\n"
  "\n  --help, -h      : Displays the help.\n";
        

#endif /* __PORTSTATS_H__ */


