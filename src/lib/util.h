/*
 * Lorenzo Saino, Massimo Gallo
 *
 * Copyright (c) 2016 Alcatel-Lucent, Bell Labs
 *
 */

#ifndef _UTIL_H_
#define _UTIL_H_

/**
 * @file
 *
 * Utility functions
 */

#include <stdint.h>
#include <rte_ip.h>

/**
 * Check if a specific bit is set in a mask.
 *
 * @param mask
 *   The bitmask
 * @param bit
 *   The index of the bit test
 *
 * @return
 *   1 if the bit is set, 0 otherwise
 */
#define IS_BIT_SET(mask, bit) (mask >> bit) & 1

/**
 * Helper function for SW computation of IPv4 checksum
 *
 * Code from DPDK test-PMD application
 */
static inline uint16_t
get_16b_sum(uint16_t *ptr16, uint32_t nr)
{
	uint32_t sum = 0;
	while (nr > 1)
	{
		sum +=*ptr16;
		nr -= sizeof(uint16_t);
		ptr16++;
		if (sum > UINT16_MAX)
			sum -= UINT16_MAX;
	}

	/* If length is in odd bytes */
	if (nr)
		sum += *((uint8_t*)ptr16);

	sum = ((sum & 0xffff0000) >> 16) + (sum & 0xffff);
	sum &= 0x0ffff;
	return (uint16_t)sum;
}

/**
 * Compute IPv4 checksum (in software) in place
 *
 * Code from DPDK test-PMD application
 *
 * @param ipv4_hdr
 *   Pointer to the IPv4 header
 */
static inline void
set_ipv4_cksum(struct ipv4_hdr *ipv4_hdr)
{
	uint16_t cksum;
	ipv4_hdr->hdr_checksum = 0;
	cksum = get_16b_sum((uint16_t*)ipv4_hdr, sizeof(struct ipv4_hdr));
	ipv4_hdr->hdr_checksum = (uint16_t)((cksum == 0xffff)?cksum:~cksum);
}

/**
 * Count bit set in a 32 bit mask
 *
 * Note: this implementation could be improved using x86 hardware population
 * count instruction or at least using the "variable-precision SWAR algorithm"
 * (http://stackoverflow.com/questions/109023/how-to-count-the-number-of-set-bits-in-a-32-bit-integer)
 *
 * However since this function is not used in data plane operation but just
 * in initialization, we don't care about performance, but favour
 * maintainability instead
 *
 * @param mask
 *   The bitmask
 *
 * @return
 *   The number of bits set in the mask
 */
uint8_t popcnt_32(uint32_t mask);

/**
 * Count bit set in a 32 bit mask
 *
 * Note: this implementation could be improved using x86 hardware population
 * count instruction or at least using the "variable-precision SWAR algorithm"
 * (http://stackoverflow.com/questions/109023/how-to-count-the-number-of-set-bits-in-a-32-bit-integer)
 *
 * However since this function is not used in data plane operation but just
 * in initialization, we don't care about performance, but favour
 * maintainability instead
 *
 * @param mask
 *   The bitmask
 *
 * @return
 *   The number of bits set in the mask
 */
uint8_t popcnt_64(uint64_t mask);

/**
 * Parse 32 bit bitmask from a string
 *
 * @param mask
 *   The string containing the mask
 *
 * @return
 *   The parsed bitmask
 */
uint32_t parse_mask_32(const char *mask);

/**
 * Parse 64 bit bitmask from a string
 *
 * @param mask
 *   The string containing the mask
 *
 * @return
 *   The parsed bitmask
 */
uint64_t parse_mask_64(const char *mask);

/**
 * Return number of ports available in system among those specified by the
 * portmask.
 *
 * A port is available if physically connected and attached to DPDK drivers
 *
 * @param portmask
 *   The portmask of ports to be used by the DPDK application
 *
 * @return
 *   The ports available among those specified in the portmask
 */
uint8_t get_nb_ports_available(uint32_t portmask);

/**
 * Get number of lcores available to the DPDK application
 */
uint8_t get_nb_lcores_available();

/**
 * Compare two Length-Value fields with 20byte length in Big-Endian (network
 * order) format
 *
 * This function takes two Lenght-Value char arrays (practically TLV arrays
 * stripped of the type byte)
 *
 * @param lv_a
 *   The first Length-value field
 * @param lv_b
 *   The second Length-value field
 *
 * @return
 *   0 if equal, other values otherwise, like memcmp
 */
int compare_lv_2_be(uint8_t *lv_a, uint8_t *lv_b);

/**
 * Parse Ethernet address from a string formatted as "XX:XX:XX:XX:XX:XX".
 *
 * @param str
 *   Pointer to the string to parse
 * @param addr
 *   Pointer to the data structure where to write the parsed address
 *
 * @return
 *   0 if parsing is successful, -1 otherwise
 */
int parse_ether_addr(char *str, struct ether_addr *addr);

#endif /* _UTIL_H_ */
