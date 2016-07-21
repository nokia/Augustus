"""
Lorenzo Saino, Massimo Gallo

Copyright (c) 2016 Alcatel-Lucent, Bell Labs


ICN packet formats implemented on Scapy and related utility functions
"""
import zlib
import struct
from scapy.all import *


__all__ = [
    'IcnInterest',
    'IcnData',
    'interest_over_ip',
    'data_over_ip',
          ]

# IP protocol field indicating ICN payload.
# In IPv4 specification, 253 is reserved for experimentation
IP_PROTO_ICN = 253

# Scapy automatically converts multiple-byte values to big-endian
TLV_TYPE_NAME_COMPONENTS_OFFSET =   0x0001
TLV_TYPE_NAME_SEGMENT_IDS_OFFSETS = 0x0002

class IcnInterest(Packet):
    """Alcatel-Lucent ICN Interest packet format

    See also:
    ---------
    http://www.ietf.org/proceedings/interim/2014/09/27/icnrg/slides/slides-interim-2014-icnrg-2-4.pdf
    """

    name = "ICN_INTEREST"

    # fmt: B: 1 byte, H: 2bytes, I: 4 bytes
    fields_desc = [
        ShortField("type", 0),
        ShortField("pkt_len", None),
        XByteField("hop_limit", 10),
        ShortField("flags", 0),
        ShortField("hdr_len", None),
        FieldLenField("chunk_name_len", None, length_of="chunk_name", fmt="H"),
        StrLenField("chunk_name", None, length_from=lambda pkt: pkt.chunk_name_len),
        XShortField("componen_type", TLV_TYPE_NAME_COMPONENTS_OFFSET),
        FieldLenField("component_offset_len", None, length_of="component_offset", fmt="H"),
        StrLenField("component_offset", None, length_from=lambda pkt: pkt.component_offset_len)
					]

    def post_build(self, p, pay):
        """Function called when packet is sent to the wire to generate byte stream

        This function sets values of header length and packet fields
        """
        hdr_len = self.hdr_len
        pkt_len = self.pkt_len
        if not hdr_len:
            hdr_len = len(p)
            p = p[:7] + chr(hdr_len >> 8) + chr(hdr_len % 256) + p[9:]
        if not pkt_len:
            pkt_len = len(p) + len(pay)
            p = p[:2] + chr(pkt_len >> 8) + chr(pkt_len % 256) + p[4:]
        return p + pay

    def answers(self, other):
        """Return 1 if other can be an answer to this packet, 0 otherwise"""
        if not isinstance(other, IcnData):
            return 0
        if other.chunk_name != self.chunk_name:
            return 0
        return 1


class IcnData(Packet):
    """Alcatel-Lucent ICN Data packet format

    See also
    --------
    http://www.ietf.org/proceedings/interim/2014/09/27/icnrg/slides/slides-interim-2014-icnrg-2-4.pdf
    """

    # fmt: B: 1 byte, H: 2bytes, I: 4 bytes
    name = "ICN_DATA"
    fields_desc = [
        ShortField("type", 1),
        ShortField("pkt_len", None),
        XByteField("hop_limit", 10),
        ShortField("flags", 0),
        ShortField("hdr_len", None),
        FieldLenField("chunk_name_len", None, length_of="chunk_name", fmt="H"),
        StrLenField("chunk_name", None, length_from=lambda pkt: pkt.chunk_name_len),
        XShortField("componen_type", TLV_TYPE_NAME_COMPONENTS_OFFSET),
        FieldLenField("component_offset_len", None, length_of="component_offset", fmt="H"),
        StrLenField("component_offset", None, length_from=lambda pkt: pkt.component_offset_len)
                  ]

    def post_build(self, p, pay):
        """Function called when packet is sent to the wire to generate byte stream

    	This function sets values of header length and packet fields
    	"""
        hdr_len = self.hdr_len
        pkt_len = self.pkt_len
        if not hdr_len:
            hdr_len = len(p)
            p = p[:7] + chr(hdr_len >> 8) + chr(hdr_len % 256) + p[9:]
        if not pkt_len:
            pkt_len = len(p) + len(pay)
            p = p[:2] + chr(pkt_len >> 8) + chr(pkt_len % 256) + p[4:]
        return p + pay


def int_to_ipv4(intval):
    """Convert an integer (32 bits) value to an IPv4 addressed represented as
    a *a.b.c.d* string

    Parameters
    ----------
    intval : int
        Integer value

    Returns
    -------
    ipv4 : str
        The string representation of the IPv6 address
    """
    l = [0, 0, 0, 0]
    for i in range(4):
        l[-i] = str(intval % 256)
        intval >>= 8
    return ".".join(l)

def compute_offset(name):
    """Return byte array with offsets of component separators

    Parameters
    ----------
    name : str
        The content name

    Returns
    -------
    offsets : str
        String of offsets
    """
    offsets = [i for i, ch in enumerate(name) if ch == '/']
    return "".join(chr(i >> 8) + chr(i % 256) for i in offsets)

def interest_over_ip(name, chunk_id, hash_prefix_only=True, dst_mac_addr=None,
                     dst_ip_addr=None, f_hash=None, component_offset=None):
    """Return a full ICN over IP Interest packet.

    The packet contains the CRC32 hash of the name as source IPv4 address and
    the destination IPv4 address is left blank.

    Parameters
    ----------
    name : str
        The ICN object name
    chunk_id : int
        The chunk ID
    hash_prefix_only : bool, optional
        If True, hash is done on the prefix of the name, i.e. only on the content object name.
        Otherwise, it is done on the entire object name, comprising also chunk ID
    dst_mac_addr : str, optional
        The destination MAC address
    dst_ip_addr : str, optional
        The destination IP address
    f_hash : callable, optional
        The hash function to use, CRC32 is used if not specified
    component_offset : str, optional
        String of component offsets. If None, it is computed by the function
    """
    if not f_hash:
        f_hash = zlib.crc32
    chunk_name = name + "/" if name[-1] != "/" else name
    chunk_name = chunk_name + struct.pack('>I', chunk_id)
    eth = Ether() if dst_mac_addr is None else Ether(dst=dst_mac_addr)
    name_hash = f_hash(name) if hash_prefix_only else f_hash(chunk_name)
    ip = IP(dst=(dst_ip_addr or "1.1.1.1"), src=int_to_ipv4(name_hash), proto=IP_PROTO_ICN)
    if not component_offset:
        component_offset = compute_offset(chunk_name)
    return eth/ip/IcnInterest(chunk_name=chunk_name, component_offset=component_offset)

def data_over_ip(name, chunk_id, payload="", hash_prefix_only=True,
                 dst_mac_addr=None, dst_ip_addr=None, f_hash=None, component_offset=None):
    """Return a full ICN over IP Data packet.

    The packet contains the CRC32 hash of the name as source IPv4 address and
    the destination IPv4 address is left blank.

    Parameters
    ----------
    name : str
        The ICN object name
    chunk_id : int
        The chunk ID
    payload : str, optional
        The payload of the data packet
    hash_prefix_only : bool, optional
        If True, hash is done on the prefix of the name, i.e. only on the content object name.
        Otherwise, it is done on the entire object name, comprising also chunk ID
    dst_mac_addr : str, optional
        The destination MAC address
    dst_ip_addr : str, optional
        The destination IP address
    f_hash : callable, optional
        The hash function to use, CRC32 is used if not specified
    component_offset : str, optional
        String of component offsets. If None, it is computed by the function
    """
    if not f_hash:
        f_hash = zlib.crc32
    chunk_name = name + "/" if name[-1] != "/" else name
    chunk_name = chunk_name + struct.pack('>I', chunk_id)
    eth = Ether() if dst_mac_addr is None else Ether(dst=dst_mac_addr)
    name_hash = f_hash(name) if hash_prefix_only else f_hash(chunk_name)
    ip = IP(dst=(dst_ip_addr or "1.1.1.1"), src=int_to_ipv4(name_hash), proto=IP_PROTO_ICN)
    if not component_offset:
        component_offset = compute_offset(chunk_name)
    return eth/ip/IcnData(chunk_name=chunk_name, component_offset=component_offset)/payload
