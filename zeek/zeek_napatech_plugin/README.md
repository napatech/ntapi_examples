# bro-napatech
Napatech 3g support for Bro.

Installation
------------

Follow Napatech's instructions to install its kernel module and userspace libraries. 

```
./configure --bro-dist=<path to bro sources> --with-napatech=<path to napatech installation> && make && make install
```

If everything built and installed correctly, you should be able to verify the installation with the following command and output:

```
bro -N Bro::Napatech
Bro::Napatech - Packet acquisition via Napatech NTAPI (dynamic, version 1.0)
```

Configuration
-------------

In order to use the plugin, you'll have to modify your Napatech configuration. Set the following values within your **ntservice.ini**:

```
 ## These values are already defined within your ntservice.ini, so make sure you replace them, don't add new ones. 
 PacketDescriptor = NT
 TimestampFormat = NATIVE_UNIX
```

**Note: in this version of bro-napatech, you must configure an ntpl script to create DYN4 packet descriptors.**

It is recommended that you comment out the ntpcap.ini completely so that it doesn't conflict with other configuration.

Finally, you'll need to write an ntpl script to define the streams that you will use. 

Usage
-----

Once installed, you can reference Napatech stream IDs by prefixing them with ``napatech::`` on the command line. 

For example, to monitor ``stream 9``:

```
bro -i napatech::9
```

To use it in production with multiple Bro processes, use a configuraiton similar to this in node.cfg:

```
[worker-1]
type=worker
host=localhost
interface=napatech::1

[worker-2]
type=worker
host=localhost
interface=napatech::2
```
