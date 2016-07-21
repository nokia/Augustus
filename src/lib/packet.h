/*
 * Lorenzo Saino, Massimo Gallo
 *
 * Copyright (c) 2016 Alcatel-Lucent, Bell Labs
 *
 */

#ifndef _PACKET_H_
#define _PACKET_H_

/**
 * @file
 *
 * ICN packet definitions
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "defaults.h"



/* Big-endian EtherType fields */
#define ETHER_TYPE_IPv4_BE 0x0008 /**< IPv4 Protocol. */
#define ETHER_TYPE_IPv6_BE 0xDD86 /**< IPv6 Protocol. */
#define ETHER_TYPE_ARP_BE  0x0608 /**< Arp Protocol. */

/* Big-endian ARP oper fields */
#define ARP_OPER_REQUEST_BE 0x0100 /**< ARP request. */
#define ARP_OPER_REPLY_BE 0x0200 /**< ARP reply. */

/**
 * packet type
 */
#define TYPE_INTEREST 0x0000
#define TYPE_DATA 0x0001
#define TYPE_CONTROL 0x0002

#define TYPE_INTEREST_BE 0x0000
#define TYPE_DATA_BE 0x0100
#define TYPE_CONTROL_BE 0x0200

/**
 * IP protocol code for ICN packet.
 *
 * 253 is assigned by IANA to research and experimentation
 */
#define IPPROTO_ICN 253

/**
 * Offset of name within ICN header
 */
#define ICN_HDR_NAME_OFFSET 11

/*
 * TLV types.
 *
 * For simplicity here we report the tag values for the 1+1 case but use only TLVs coded in 2+2 format.
 */

/* Little-endian version */
#define TLV_TYPE_NAME_COMPONENTS_OFFSET					0x0001
#define TLV_TYPE_NAME_SEGMENT_IDS_OFFSETS 				0x0002

/* Big-endian version */
#define TLV_TYPE_NAME_COMPONENTS_OFFSET_BE 				0x0100
#define TLV_TYPE_NAME_SEGMENT_IDS_OFFSETS_BE 				0x0200
#define TLV_TYPE_INTEREST_NONCE_BE 					0x0300



/**
 * ICN packet format definition
 *
 * Note: the __packed__ attribute ensures that every architecture aligns
 * components to 1 byte boundaries. As a result it will be possible to parse
 * a received packet (in the format of a byte array) just by casting it to
 * struct icn_hdr
 *
 * Packet structure is similar to the one described in source: http://systemx.enst.fr/content-packets-alu.html
 */
struct icn_hdr {
    uint16_t             type;                    /*Type of packet INTEREST/DATA*/
    uint16_t             pkt_len;                 /*Total packet Len*/
    uint8_t              hop_limit;               /*Hop limit to limit the cope of pkts*/
    uint16_t              flags;                   /*Flags used to modify fixed header*/
    uint16_t             hdr_len;                 /*Indicates fixed header length*/
} __attribute__((__packed__));

/**
 * Data structure storing metadata related to an ICN name
 *
 * It stores number of components and offsets of each component and CRC hashes
 * of first component.
 *
 * This data structure is populated when the name is parsed. 
 * This structure ensures that hashes are not recalculated every time a lookup 
 * is performed for a different prefix length.
 */
struct icn_packet {
    struct icn_hdr*  	 hdr;
    uint8_t*             pkt;
    uint16_t             name_len;
    uint8_t*             name;
    uint8_t*             component_offsets;       /*Pointer to the beginning of the Value in the component offset's TLV*/
    uint16_t             component_nr;            /*Number of name's components*/
    uint16_t             component_offsets_size;  /*Size of a single component offset in the Segment ID's TLV*/
    uint8_t*		 payload;
    uint32_t		 lpm_crc; /**< CRC32 hash of name LPM */
    uint32_t		 crc[MAX_NAME_COMPONENTS];
}__attribute__((__packed__));


/**
 * Parse the icn packet
 *
 * @param pkt
 *   Pointer to the packet to parse
 * @param icn_pkt pointer to an empty data structure of the type icn_packet that will be filled with the parsed packet
 *
 * @return
 * 	- 0 if packet has been parsed successfully,
 * 	- 1 otherwise
 */
uint8_t parse_packet(uint8_t* pkt, struct icn_packet * icn_pkt);


#endif /* _PACKET_H_ */
