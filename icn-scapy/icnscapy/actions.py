"""
Lorenzo Saino, Massimo Gallo

Copyright (c) 2016 Alcatel-Lucent, Bell Labs


Simple test functions for testing ICN routers

This module contains functions for generating simple packets or sequence of
packets for the purpose of testing protocol compliance and robustness of ICN
router implementations
"""
from scapy.all import *

from icnscapy import interest_over_ip, data_over_ip

__all__ = [
    'send_interest',
    'send_seq_interests',
    'send_data'
    ]

# Suppress all log messages printed out by Scapy
conf.verb = 0

def send_interest(name, chunk_id, hash_prefix_only=True, iface=None,
                  dst_mac_addr=None, dst_ip_addr=None, component_offset=None):
    """Issue an Interest packet for content *name* with chunk_id

    Parameters
    ----------
    name : str
        Name of object to request
    chunk_id : int
        Sequence number of the chunk to request
    hash_prefix_only : bool, optional
        If True compute hash only on object name, excluding chunk ID
    iface : str, optional
        The interface from which the packet must be transmitted
    dst_mac_addr : str, optional
        The destination MAC address
    dst_ip_addr : str, optional
        The destination IP address
    component_offset : str, optional
        String of component offsets. If None, it is computed by the function
    """
    sendp(interest_over_ip(name, chunk_id, hash_prefix_only=hash_prefix_only,
                           dst_mac_addr=dst_mac_addr, dst_ip_addr=dst_ip_addr,
                           component_offset=component_offset), iface=iface)

def send_seq_interests(name, n_chunks, hash_prefix_only=True, iface=None,
                       dst_mac_addr=None, dst_ip_addr=None, component_offset=None):
    """Request content *name* from chunk *1* to *n_chunks* sequentially

    Parameters
    ----------
    name : str
        Name of object to request
    n_chunks : int
        Number of chunks to issue
    hash_prefix_only : bool, optional
        If True compute hash only on object name, excluding chunk ID
    iface : str, optional
        The interface from which the packet must be transmitted
    dst_mac_addr : str, optional
        The destination MAC address.
    dst_ip_addr : str, optional
        The destination IP address
    component_offset : str, optional
        String of component offsets. If None, it is computed by the function
    """
    for i in range(n_chunks):
        sendp(interest_over_ip(name, i, hash_prefix_only=hash_prefix_only,
                               dst_mac_addr=dst_mac_addr, dst_ip_addr=dst_ip_addr,
                               component_offset=component_offset), iface=iface)

def send_data(name, chunk_id, payload="", hash_prefix_only=True, iface=None,
              dst_mac_addr=None, dst_ip_addr=None, component_offset=None):
    """Issue a Data packet for content *name* with chunk_id and paylod *payload*

    Parameters
    ----------
    name : str
        Name of object to request
    chunk_id : int
        Sequence number of the chunk to request
    payload : str
        The payload of the Data packet
    hash_prefix_only : bool, optional
        If True compute hash only on object name, excluding chunk ID
    iface : str, optional
        The interface from which the packet must be transmitted
    dst_mac_addr : str, optional
        The destination MAC address.
    dst_ip_addr : str, optional
        The destination IP address
    component_offset : str, optional
        String of component offsets. If None, it is computed by the function
    """
    sendp(data_over_ip(name, chunk_id, payload=payload, hash_prefix_only=hash_prefix_only,
                       dst_mac_addr=dst_mac_addr, dst_ip_addr=dst_ip_addr,
                       component_offset=component_offset), iface=iface)
