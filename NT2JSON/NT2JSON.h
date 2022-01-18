
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


