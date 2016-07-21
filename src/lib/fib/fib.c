/*
 * Lorenzo Saino, Massimo Gallo
 *
 * Copyright (c) 2016 Alcatel-Lucent, Bell Labs
 *
 */

#include "fib.h"

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>

#include <rte_hash_crc.h>
#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_malloc.h>

#include "fib_hash_table.h"
#include "../packet.h"




fib_t* fib_create(uint32_t num_buckets, uint32_t max_elements, uint32_t bf_size, int socket) {
    fib_t *fib;
    void *p;
    /* Allocate FIB. Not the actual FIB just a pointer to PBF and pointer to HT */
	p = rte_zmalloc_socket("FIB", sizeof(fib_t), RTE_CACHE_LINE_SIZE, socket);
	if (p == NULL) {
		// If p is NULL, then something went wrong because either the arguments
		// were incorrect or not enough memory is available
		return NULL;
	}
    fib = (fib_t *) p;
    
    fib->table = fib_hash_table_create(num_buckets, max_elements, socket);
    if (fib->table == NULL) {
    	fib_free(fib);
    	return NULL;
    }
    return fib;
}


void fib_free(fib_t *fib) {
	if(fib == NULL) {
		return;
	}
	if(fib->table != NULL) {
		fib_hash_table_free((void *)fib->table);
	}
	rte_free((void *)fib);
	return;
}


int8_t fib_add(fib_t *fib, uint8_t *name, uint16_t name_len, uint8_t face) {
	int8_t ret;
	if(unlikely(name_len == 0 || name[0] == '\0')) {
		return -EINVAL;
	}
	ret = fib_hash_table_add_key(fib->table, name, name_len, face);
	if (ret < 0) {
		return ret;
	}
	return 0;
}

int8_t fib_del(fib_t *fib, uint8_t *name, uint16_t name_len, uint8_t face){
	int8_t ret;
	if(unlikely(name_len == 0 || name[0] == '\0')) {
		return -EINVAL;
	}
	ret = fib_hash_table_del_key(fib->table, name,name_len, face);
	if (ret < 0) {
		return ret;
	}
	return ret;
}

int8_t fib_lookup(fib_t *fib, struct icn_packet *icn_packet) {
	int16_t comp, res;
	for (comp = icn_packet->component_nr-1; comp >= 0; comp--) {
		uint16_t offset = rte_be_to_cpu_16(((uint16_t*)icn_packet->component_offsets)[comp])+1;
		icn_packet->crc[comp]=rte_hash_crc(icn_packet->name, offset, MASTER_CRC_SEED);
		res = fib_hash_table_lookup_with_hash(fib->table, icn_packet->name, offset, icn_packet->crc[comp]);

		if(res >= 0) {
			/*
			 * There is actually a hash table entry at the prefix len 
			 * Return the ID of the next hop
			 */
			return res;

		}
		/*
		 * Reach this piece of code only if there has been a Bloom filter
		 * false positive. Just continue with the cycle with a shorter prefix
		 */
	}
	/*
	 * Reach this piece of code only if no entries were found in the FIB.
	 * Return error
	 */

	return -ENOENT;
}
