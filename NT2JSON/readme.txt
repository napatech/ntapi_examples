NT2JSON querys any Napatech adapters and outputs the values of many of the sensors and statistic counters that
are available on the card to JSON output.

Prerequisites:

This program utilizes the json-c libraries for formatting the JSON output.  This is available directly for download 
in several repositories and the source is available here: https://github.com/json-c/json-c.


Output Structure:

The general outline of the output is as follows:

    system

    adapters
        TimeSync
        Sensors

    PortInfo
        Port
            info
            stats
                Rx
                    RMON
                    ExtRmon
                    ExtDrop
                    Decode
                    Checksum
                Tx
                    RMON
                    ExtRmon

    StreamData
        stream
            info
            stats
    
The file sampleOutput.json provides an example of what the output might look like.
However, as different adapters support different options the specific output 
will vary.



