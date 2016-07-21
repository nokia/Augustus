# ICN Scapy

ICN Scapy is a Python package based on Scapy packet manipulation tool that allows
users to format and send ICN packets. This tool is meant to be used at low speed
to test basic cases with specially formatted packets.


# Requirements

To use ICN Scapy you need:
 * Python 2.7 or later version
 * [Scapy](http://www.secdev.org/projects/scapy/)

In addition, if you need to send Data packets, make sure you have Jumboframes
enabled using ifconfig. Example:

    sudo ifconfig ethX mtu 9000


# Usage
To use ICN Scapy, you don't need any installation, just move to this folder and
launch the Python interpreter in superuser mode.

    $ sudo python

Superuser mode is required to send packets on raw sockets.

After having launched Python, import icnscapy package:

    >>> import icnscapy

And then launch commands. For example to issue an Interest for A/B/C on
interface eth1, use the following command:

    >>> icnscapy.send_interest("A/B/C", iface="eth1")

For further information about the functions available, consult the documentation
in the `icnscapy.actions` and `icnscapy.packet` modules.