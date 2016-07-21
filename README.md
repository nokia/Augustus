# Augustus: DPDK content router

README for the DPDK content router

## Requirements
To use the DPDK content router you need a machine with:
 * Linux Ubuntu
 * Intel DPDK framework
 * A NIC supported by Intel DPDK

Augustus has been successfully tested with:
 * Ubuntu 14.04 and Ubuntu 16.04 
 * dpdk-2.2.0 and dpdk-16.04 (the last two releases).

However, when updating Ubuntu, notice that there are some issues with gcc 5 and the native dpdk config which were fixed post dpdk-2.2.0 
(https://www.mail-archive.com/ubuntu-bugs@lists.ubuntu.com/msg4943360.html).

## Set up
To start, download and install the DPDK library following the DPDK instructions for compiling the target 'x86_64-native-linuxapp-gcc'. 
Before compiling and running Augustus: 
 * insert igb_uio module
 * enable huge pages
 * attach cards to the igb_uio module.

All these steps can be performed by running the following script, provided by Intel DPDK: `<DPDK_DIR>/tools/setup.sh`
where `<DPDK_DIR>` is the directory where you downloaded Intel DPDK.
Please refer to the Intel DPDK manual for further information.


## Build
To build the DPDK software router, on machines where DPDK is installed, go to the folder where this README file is located 
and run the following commands:

    export export RTE_TARGET=x86_64-native-linuxapp-gcc
    export export RTE_SDK=<DPDK_DIR>

replacing `<DPDK_DIR>` with the path where DPDK is located.

Then run:

    make clean
    make

These will build the Augustus DPDK content router. The executable file of the content router is located in the `build` directory.
Make sure to always run `make clean` before `make`.

## Run
After building the content router you can run it with the following command

    sudo build/dpdk-content-router -c COREMASK -n MEM_CHANNEL -- -p PORTMASK -m "MAC1 MAC2" [-P] 
    
    Usage:	[EAL options] -- Options
		
		[EAL options]: -c COREMASK -n MEM_CHANNEL
		Options:
			-p PORTMASK:                 Hexadecimal bitmask of ports to configure\n"
			-m MAC0 [MAC1 .. MACN]:      list of MAC addresses associated to port0, port1, ..., portN (separated by a space)\n"
			-P                           Enable promiscuous mode\n"
			--no-numa                    Disable NUMA awareness\n"
			-h --help                    Show this help\n"
			-v, --version                Show version\n",
    
    
Example with 2 ports, 3 cores:

    sudo build/dpdk-content-router -c 0x7 -n 2 -- -p 0x3 -P -m "5c:b9:01:88:f0:88 5c:b9:01:88:f0:89"
    
All arguments before the `--` sign are DPDK EAL arguments and those after are arguments specific for the DPDK content 
router.

Arguments:
 * `COREMASK` is the hexadecimal bitmask of the CPU cores to use in the application.
 * `MEM_CHANNEL` is the number of memory channels used by the DRAM banks. DPDK uses this information to align packet memory
   buffer in such a way that contiguous packets are allocated in memory regions accessed by different channels. In such a way, 
   prefetching consecutive packets is faster cause different channels are used.
 * `PORTMASK` is the hexadecimal bitmask of enabled Ethernet ports.
 * `P`: if specified, enables promiscuous mode on NICs

# Configuration
Most of the configurations such as CS, FIB, PIT size, number of buckets, etc. are stored in `defaults.h`
Use `config.h` to to override default parameters and re-build Augustus to apply the changes.

To modify FIB use build fib-ctrl. It sends an update command to Augustus from the specified address.

Usage:

	fib_ctrl -a 'address' -c "command"

where `command` is of the format `(ADD,CLR,DEL):prefix_name:port_id`
	 
Example:

	sudo build/fib-ctrl -a '127.0.0.1' -c "ADD:a/b/c/d/e/f/:0"

## Debug and optimized mode
Throughout the Augustus code there are some logging macros that print logging information to standard output for debugging 
purposes. These macros are useful when running Augustus with limited load for debugging purposes only. For high speed tests 
they must be removed. This must be done by compiling the DPDK library with the proper debug level.

To do so, open the source tree of the DPDK library (the actual DPDK, not Augustus). Move to the config dir and in the 
`common_linuxapp` set `RTE_LOG_LEVEL` to 4 instead of the default 8. Rebuild the DPDK library and rebuild Augustus. That's it.

A good advice is to keep on the target machine several copies of DPDK each compiled with different debug levels and configuration 
and change the value of the RTE_SDK environment variable each time a switch in configuration is required.

A more elegant way but possibly more error-prone is to create a new defconfig file in the config folder for each desired 
configuration named 
`defconfig_MYTARGET` and then build DPDK each time specifying `MYTARGET` as value of the `RTE_TARGET` environment variable. In 
this way there are separate copies only of the build files, not of the source as well, but may generate confusion.

Also to run the content router in DPDK, you may consider editing the Makefile to use the GCC compiler with the `-O0` argument,
which produced optimized binary code.

## Print and reset stats
Each core keeps statistics for results collection. Variables storing statistics are local to each thread and, for performance 
reasons, there is no concurrent access control on them, not even atomic read/write operations. Statistics can be printed to 
standard output or reset when there is no traffic being handled by the content router. This can be done by sending user-specific 
signals to the content router process.

To print stats to standard output, send a USR1 signal to the router process. When the process is running, open another terminal 
and type:

    sudo killall -SIGUSR1 dpdk-content-router

This command will print on the stdout stream attached to the content router, the per-core and cumulative statistics.

To reset statistics counter, send a USR2 signal instead:

    sudo killall -SIGUSR2 dpdk-content-router 

## Build documentation
Each function, macro, data structure and typedef is documented. Function documentation is present only in the prototype declaration 
(i.e., in header files). The documentation is written in Doxygen format and, using Doxygen, it is possible to build an API definition 
in HTML format. This can be done with the Makefile, by executing:

    make doc
    
Before building docs, be sure to have doxygen installed:

    sudo apt-get install doxygen
    
## Caveats
Here is a list of caveats to be aware of about the implementation of the Augustus DPDK content router.
 * No chaining on the FIB hash-table. Each bucket supports 7 entries. If you use a FIB that overflows a bucket, just create a hash
   table with more buckets
 * No cryptographic signatures of Interest and Data packets
 * The FIB lookup is very simplistic: the lookup algorithms first checks if there is a match at `num_prefix`, then checks if match
   at `num_prefix-1` and so forth.
 * If the FIB is modified while Interest packets are being processed, there might be inconsistent behavior as there is no synchronization
   mechanism implemented to ensure correctness of concurrent lookup and update operations.
 * In order to exploit nic's RSS, name's hash is embedded in the ip destination address (In the future it can be embedded in the UDP port 
   and IP addresses used to identify the port)
