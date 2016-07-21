/*
 * Lorenzo Saino, Massimo Gallo
 *
 * Copyright (c) 2016 Alcatel-Lucent, Bell Labs
 *
 */

#include <string.h>
#include <stdio.h>

#include <rte_hash_crc.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_mbuf.h>

#include <config.h>

#include "cs.h"


cs_t* cs_create(int num_buckets, int max_elements, int socket) {
	cs_t *cs;
	void *p;
	
	// Allocate on the specified NUMA node. If socket is SOCKET_ID_ANY, then it
	// allocates the hash table on the socket of the calling lcore
	p = rte_zmalloc_socket("CS", sizeof(cs_t), RTE_CACHE_LINE_SIZE, socket);
	if(p == NULL) {
		return NULL;
	}
	cs = (cs_t*) p;

	cs->num_buckets = num_buckets;
	/*
	 * Note: here cs->max_elements is set to one integer more to distinguish
	 * between the case in which the ring is full and the ring is empty because
	 * in both cases top and bottom pointers would overlap. 
	 *
	 */
	cs->max_elements = max_elements + 1;
	printf("CS size %d\n", cs->max_elements);

	/* Allocate space for the actual hash-table */
	p = rte_zmalloc_socket("CS_TABLE", cs->num_buckets*sizeof(struct cs_bucket),
			RTE_CACHE_LINE_SIZE, socket);
	if(p == NULL) {
		return NULL;
	}
	cs->table = (struct cs_bucket*) p;

	/* Allocate space for the ring */
	p = rte_zmalloc_socket("PIT_RING", cs->max_elements*sizeof(struct cs_entry),
			RTE_CACHE_LINE_SIZE, socket);
	if(p == NULL) {
		return NULL;
	}
	cs->ring = (struct cs_entry *) p;
	return cs;
}


static inline
int8_t __cs_insert_with_hash(cs_t *cs, uint8_t *name, uint8_t name_len,
		struct rte_mbuf *mbuf, uint32_t crc) {
	uint32_t bucket;
	uint8_t entry;
	
	/* Get index of corresponding bucket */
	bucket = crc % cs->num_buckets;
	/* Iterate all buckets till find one free and insert */
	for (entry = 0; entry < BUCKET_SIZE; entry++) {
		if (likely(cs->table[bucket].busy[entry] == 0)) {
			/* if full, evict a content, in FIFO fashion*/
			if(likely(is_cs_full(cs))) {
				cs->table[cs->ring[cs->bottom].bucket].busy[cs->ring[cs->bottom].tab] = 0;
				cs->ring[cs->bottom].active = 0;
				/* Free pointer to mbuf holding the actual packet */
				rte_pktmbuf_free(cs->ring[cs->bottom].mbuf);
				if(unlikely(cs->bottom == cs->max_elements - 1)) {
					cs->bottom = 0;
				} else {
					cs->bottom++;
				}
			}
			/* Now insert new content */
			cs->table[bucket].busy[entry] = 1;
			cs->table[bucket].entry[entry].crc = crc;
			cs->table[bucket].entry[entry].index = cs->top;
			cs->ring[cs->top].active = 1;
			cs->ring[cs->top].bucket = bucket;
			cs->ring[cs->top].tab = entry;
			cs->ring[cs->top].name_len = name_len;
			rte_memcpy(cs->ring[cs->top].name, name, name_len);
			cs->ring[cs->top].mbuf = mbuf;
			cs->top = (cs->top + 1) % cs->max_elements;
			return 0;
		}
	}
	return -ENOSPC;
}


int8_t cs_insert_with_hash(cs_t *cs, uint8_t *name, uint8_t name_len,
		struct rte_mbuf *mbuf, uint32_t crc) {
	return __cs_insert_with_hash(cs, name, name_len, mbuf, crc);
}


int8_t cs_insert(cs_t *cs, uint8_t *name, uint8_t name_len, struct rte_mbuf *mbuf) {
	uint32_t crc = rte_hash_crc(name, name_len, MASTER_CRC_SEED);
	return __cs_insert_with_hash(cs, name, name_len, mbuf, crc);
}


struct rte_mbuf *__cs_lookup_with_hash(cs_t *cs, uint8_t *name,
		uint8_t name_len, uint32_t crc) {
	uint32_t bucket;
	uint8_t entry;
	/* Get index of corresponding bucket */
	bucket = crc % cs->num_buckets;
	/* Iterate all buckets till find one free and insert */
	for (entry = 0; entry < BUCKET_SIZE; entry++) {
		if (likely(cs->table[bucket].busy[entry] == 0)) {
			continue;
		}
		if(unlikely(cs->table[bucket].entry[entry].crc != crc)) {
			continue;
		}
		/* found element with matching CRC, now verify if name matches */
		if(unlikely(name_len != cs->ring[cs->table[bucket].entry[entry].index].name_len)) {
			/* name lengths do not match, keep iterating bucket */
			continue;
		}
		if (unlikely(memcmp(name, cs->ring[cs->table[bucket].entry[entry].index].name, name_len)) != 0) {
			/* name lengths do not match, keep iterating bucket */
			continue;
		}
		/* Element found. Remove it and return pointer to face bitmask */
		return cs->ring[cs->table[bucket].entry[entry].index].mbuf;
	}
	return NULL;
}


struct rte_mbuf *cs_lookup_with_hash(cs_t *cs, uint8_t *name, uint8_t name_len,
		uint32_t crc) {
	return __cs_lookup_with_hash(cs, name, name_len, crc);
}


struct rte_mbuf *cs_lookup(cs_t *cs, uint8_t *name, uint8_t name_len) {
	uint32_t crc = rte_hash_crc(name, name_len, MASTER_CRC_SEED);
	return __cs_lookup_with_hash(cs, name, name_len, crc);
}


void cs_free(cs_t *cs) {
	if(cs == NULL) {
		return;
	}
	
	uint8_t entry;
	uint32_t bucket;
	for (bucket = 0; bucket < CS_NUM_BUCKETS; bucket++) {
		/* Iterate all buckets*/
		for (entry = 0; entry < BUCKET_SIZE; entry++) {
			if (cs->table[bucket].busy[entry] == 0)
				continue;
			rte_free(cs->ring[cs->table[bucket].entry[entry].index].mbuf);
		}
	}

	if(cs->table != NULL) {
		rte_free(cs->table);
	}
	if(cs->ring != NULL) {
		rte_free(cs->ring);
	}
	rte_free(cs);
	return;
}
