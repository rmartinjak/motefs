motefs
======

FUSE file system to communicate with TinyOS motes. Consists of two parts,
a *nesC component* to be put on a TinyOS mote and the *fuse* file system that
talks to it via the serial port (or a serial forwarder).


fuse/
-----

Contains the `fuse` file system. Some files from `$TOSROOT/support/sdk/c/sf`
need to be compiled and copied to `fuse/tos` in order to build it
(see `fuse/tos/README`).


nesc/
-----

Contains the TinyOS component (in `MoteFS/`). An example application is also
provided, the usage of the MoteFS component should be quite obvious from it.
