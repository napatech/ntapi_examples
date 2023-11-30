# ntnetflow

## Introduction

Currently the application passes through the data from one port to another.
The flow statistics are calculated in hardware with the exception of when
they are retrieved, when there needs to be a software fallback.

## Usage

The default parameters poll the ongoing flows every 10s, and defaults
to using port 0 and port 1.  The command line arguments will allow you
to reconfigure it: more information is available using `--help`.

## Limitations

- IPv6 isn't quite supported yet in this demo (lack of the ability to print the IPs),
but it's supported by the adapter.  The filters for IPv6 are also setup.
- There's no interface for capturing/monitoring flows of interest as yet.
- Flowtable needs to be tuned, and some more optimisations.
- Try with ./ntnetflow -s -n 16 -x 16 -a 4 -b 16 -p 60
