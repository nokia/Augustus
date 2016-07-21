/*
 * Lorenzo Saino, Massimo Gallo
 *
 * Copyright (c) 2016 Alcatel-Lucent, Bell Labs
 *
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>

#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_byteorder.h>
#include <rte_mbuf.h>

#include <packet.h>
#include <config.h>

#include "util.h"


uint8_t popcnt_32(uint32_t mask) {
	uint8_t res;
	uint8_t i;
	for(i = 0, res = 0; i < 32; i++) {
		if(IS_BIT_SET(mask, i)) {
			res++;
		}
	}
	return res;
}

uint8_t popcnt_64(uint64_t mask) {
	uint8_t res;
	uint8_t i;
	for(i = 0, res = 0; i < 64; i++) {
		if(IS_BIT_SET(mask, i)) {
			res++;
		}
	}
	return res;
}

uint8_t get_nb_ports_available(uint32_t portmask) {
	uint8_t port_id, nb_ports, nb_ports_available;
	nb_ports = rte_eth_dev_count();
	for (port_id = 0, nb_ports_available = 0; port_id < nb_ports; port_id++) {
		if ((portmask & (1 << port_id)) != 0) {
			nb_ports_available++;
		}
	}
	return nb_ports_available;
}

uint8_t get_nb_lcores_available() {
	uint8_t lcore_id, nb_lcores;
	for (lcore_id = 0, nb_lcores = 0; lcore_id < APP_MAX_LCORES; lcore_id++) {
		if(rte_lcore_is_enabled(lcore_id) == 1) {
			nb_lcores++;
		}
	}
	return nb_lcores;
}

uint32_t parse_mask_32(const char *mask) {
	char *end = NULL;
	uint32_t num;
	num = strtoul(mask, &end, 16);
	if ((mask[0] == '\0') || (end == NULL) || (*end != '\0') || num == 0) {
		return 0;
	}
	return num;
}

uint64_t parse_mask_64(const char *mask) {
	char *end = NULL;
	uint64_t num;
	num = strtoull(mask, &end, 16);
	if ((mask[0] == '\0') || (end == NULL) || (*end != '\0')) {
		return 0;
	}
	return (uint64_t)num;
}

int parse_ether_addr(char *str, struct ether_addr *addr) {
	int ret;
	ret = sscanf(str, "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx",
			&addr->addr_bytes[0], &addr->addr_bytes[1], &addr->addr_bytes[2],
			&addr->addr_bytes[3], &addr->addr_bytes[4], &addr->addr_bytes[5]);
	return (ret == 6) ? 0 : -1;
}

int compare_lv_2_be(uint8_t *lv_a, uint8_t *lv_b) {
	size_t len;
	if (*lv_a != *lv_b | *(lv_a + 1) != *(lv_b + 1)) {
		// Length values not matching, hence values do not match either
		return -1;
	}
	len = (size_t) rte_be_to_cpu_16(*(uint16_t *) lv_a);
	return memcmp(lv_a + 2, lv_b + 2, len);
}
