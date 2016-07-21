/*
 * Lorenzo Saino, Massimo Gallo
 *
 * Copyright (c) 2016 Alcatel-Lucent, Bell Labs
 *
 */

#include <string.h>
#include <stdio.h>

#include <inttypes.h>

#include <rte_hash_crc.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_branch_prediction.h>
#include <rte_cycles.h>
#include <rte_common.h>

#include <config.h>

#include "pit.h"


pit_t* pit_create(int num_buckets, int max_elements, int socket, uint64_t ttl_us) {
	pit_t *pit;
	void *p;
	// Allocate on the specified NUMA node. If socket is SOCKET_ID_ANY, then it
	// allocates the hash table on the socket of the calling lcore
	p = rte_zmalloc_socket("PIT", sizeof(pit_t), RTE_CACHE_LINE_SIZE, socket);
	if(p == NULL) {
		return NULL;
	}
	pit = (pit_t*) p;

	pit->num_buckets = num_buckets;
	/*
	 * Note: here pit->max_elements is set to one integer to distinguish
	 * between the case in which the ring is full and the ring is empty because
	 * in both cases top and bottom pointers would overlap. Anyway, in this case
	 * one slot is always kept  unused.
	 */
	pit->max_elements = max_elements + 1;

	/* Allocate space for the actual hash-table */
	p = rte_zmalloc_socket("PIT_TABLE", pit->num_buckets*sizeof(struct pit_bucket),
			RTE_CACHE_LINE_SIZE, socket);
	if(p == NULL) {
		return NULL;
	}
	pit->table = (struct pit_bucket*) p;

	/* Allocate space for the ring */
	p = rte_zmalloc_socket("PIT_RING", pit->max_elements*sizeof(struct pit_entry),
			RTE_CACHE_LINE_SIZE, socket);
	if(p == NULL) {
		return NULL;
	}
	pit->ring = (struct pit_entry *) p;
	pit_set_ttl_us(pit, ttl_us);
	return pit;
}


struct pit_entry *pit_lookup(pit_t *pit, uint8_t *name, uint8_t name_len,uint32_t crc) {
	uint32_t bucket;
	uint8_t entry;
	/* Get index of corresponding bucket */
	bucket = crc % pit->num_buckets;
	/* Iterate all buckets till find one free and insert */
	for (entry = 0; entry < BUCKET_SIZE; entry++) {
		if (unlikely(pit->table[bucket].busy[entry] == 0)) {
			/* Empty entry, unlikely because that would mean there is a hole*/
			continue;
		}
		if(unlikely(pit->table[bucket].entry[entry].crc != crc)) {
			continue;
		}
		if(likely(name_len == pit->ring[pit->table[bucket].entry[entry].index].name_len)) {
			if (memcmp(name, pit->ring[pit->table[bucket].entry[entry].index].name, name_len) == 0) {
				return &(pit->ring[pit->table[bucket].entry[entry].index]);
			}
		}
	}
	/*
	 * This line is reached if the searched elements in not present in
	 * the hash table
	 */
	return NULL;
}

static inline
int8_t __pit_lookup_and_update_with_hash(pit_t *pit,
		uint8_t *name, uint8_t name_len, 
		uint8_t face, uint64_t *curr_time, uint32_t crc) {
	uint32_t bucket;
	uint8_t entry;
	uint8_t free_tab = 0xFF;
	/* Get index of corresponding bucket */
	bucket = crc % pit->num_buckets;
	/* Iterate all buckets till find one free and insert */
	for (entry = 0; entry < BUCKET_SIZE; entry++) {
		if (likely(pit->table[bucket].busy[entry] == 0)) {
			/* Empty entry */
			if(entry < free_tab) {
				free_tab = entry;
			}
			continue;
		}
		if(unlikely(pit->table[bucket].entry[entry].crc != crc)) {
			/*
			 * Found entry in bucket but CRC does not match,
			 * keep iterating bucket
			 */
			continue;
		}
		/* Found CRC matching, now let's check if name matches */
		if(unlikely(name_len != pit->ring[pit->table[bucket].entry[entry].index].name_len)) {
			/* name lengths don't match, keep iterating bucket */
			continue;
		}
		/* name length matches, let's now check the actual name */
		if (unlikely(memcmp(name, pit->ring[pit->table[bucket].entry[entry].index].name, name_len) != 0)) {
			/* names do not match, keep iterating bucket */
			continue;
		}
		/* found a matching entry, now check */
		
		/*
		 * if the face Interest came from is not in the entry yet, add it
		 */
		if((pit->ring[pit->table[bucket].entry[entry].index].face_bitmask
				& (uint64_t) (1 << face)) == 0) {
			pit->ring[pit->table[bucket].entry[entry].index].face_bitmask
				|= (1 << face);
		}
		
		/*
		 * This indicates to the caller that the Interest does not need to be
		 * forwarded cause since it is in the PIT it was already forwarded.
		 */
		return 0;
	}
	
	/*
	 * This block is reached if the searched elements in not present in
	 * the hash table and, hence, need to be inserted
	 */
	if(free_tab == 0xFF || is_pit_full(pit)) {
		return -ENOSPC;
	}
	/*
	 * Now, insert the item because it is not in the PIT and there is
	 * space to insert it.
	 */
	pit->table[bucket].busy[free_tab] = 1;
	pit->table[bucket].entry[free_tab].crc = crc;
	pit->table[bucket].entry[free_tab].index = pit->top;
	pit->ring[pit->top].active = 1;
	pit->ring[pit->top].bucket = bucket;
	pit->ring[pit->top].tab = free_tab;
	
	if(likely(curr_time == NULL)) {
		pit->ring[pit->top].expiry = get_curr_time() + pit->ttl;
	} else {
		pit->ring[pit->top].expiry = *curr_time + pit->ttl;
	}
	pit->ring[pit->top].name_len = name_len;
	rte_memcpy(pit->ring[pit->top].name, name, name_len);
	pit->ring[pit->top].face_bitmask = (1 << face);
	pit->top = (pit->top + 1) % pit->max_elements;
	/*
	 * Inform the caller that a new item was inserted and needs to be
	 * forwarded on
	 */
	return 1;
}


int8_t pit_lookup_and_update_with_hash(pit_t *pit, uint8_t *name, uint8_t name_len,  uint8_t face, uint64_t *curr_time, uint32_t crc) {
	return __pit_lookup_and_update_with_hash(pit, name, name_len, face, curr_time, crc);
}


int8_t pit_lookup_and_update(pit_t *pit, uint8_t *name, uint8_t name_len, uint8_t face, uint64_t *curr_time) {
	uint32_t crc = rte_hash_crc(name, name_len, MASTER_CRC_SEED);
	return __pit_lookup_and_update_with_hash(pit, name, name_len, face, curr_time, crc);
}


uint64_t __pit_lookup_and_remove_with_hash(pit_t *pit, uint8_t *name,
		uint8_t name_len, uint32_t crc) {
	uint32_t bucket;
	uint8_t entry;
	/* Get index of corresponding bucket */
	bucket = crc % pit->num_buckets;
	/* Iterate all buckets till find one free and insert */
	for (entry = 0; entry < BUCKET_SIZE; entry++) {
		if (likely(pit->table[bucket].busy[entry] == 0)) {
			continue;
		}
		if(unlikely(pit->table[bucket].entry[entry].crc != crc)) {
			continue;
		}
		/* found element with matching CRC, now verify if name matches*/
		if(unlikely(name_len != pit->ring[pit->table[bucket].entry[entry].index].name_len)) {
			/* name lengths do not match, keep iterating bucket */
			continue;
		}
		if (unlikely(memcmp(name, pit->ring[pit->table[bucket].entry[entry].index].name, name_len)) != 0) {
			/* name lengths do not match, keep iterating bucket */
			continue;
		}
		/* Element found. Remove it and return pointer to face bitmask */
		pit->table[bucket].busy[entry] = 0;
		pit->ring[pit->table[bucket].entry[entry].index].active = 0;
		if(pit->bottom == pit->table[bucket].entry[entry].index) {
			pit->bottom = (pit->bottom + 1) % pit->max_elements;
		}
		return pit->ring[pit->table[bucket].entry[entry].index].face_bitmask;
	}
	/*
	 * This block is reached if the searched elements in not present in
	 * the hash table. This cannot be confused by the caller with a valid
	 * face bitmask, because it would need to have at least 1 bit set.
	 */
	return 0;
}


uint64_t pit_lookup_and_remove_with_hash(pit_t *pit, uint8_t *name, uint8_t name_len, uint32_t crc) {
	return __pit_lookup_and_remove_with_hash(pit, name, name_len, crc);
}


uint64_t pit_lookup_and_remove(pit_t *pit, uint8_t *name, uint8_t name_len) {
	uint32_t crc = rte_hash_crc(name, name_len, MASTER_CRC_SEED);
	return __pit_lookup_and_remove_with_hash(pit, name, name_len, crc);
}


static inline
uint32_t __pit_purge_expired_with_time(pit_t *pit, uint64_t *curr_time) {
	uint32_t purged = 0;
	while(likely(!is_pit_empty(pit))) {
		if(unlikely(pit->ring[pit->bottom].active == 1)) {
			/* Case of active and still valid entry */
			if(likely(pit->ring[pit->bottom].expiry > *curr_time)) {
				return purged;
			}
			/* Case of active entry but expired, clean both in ring and in table */
			pit->ring[pit->bottom].active = 0;
			pit->table[pit->ring[pit->bottom].bucket].busy[pit->ring[pit->bottom].tab] = 0;
		}
		/*
		 * We assume that if the entry in the ring is flagged as inactive,
		 * also the busy flag in the bucket is properly unset, so we do not go
		 * and unset it. If this was not the case, then something is wrong
		 */
		if(unlikely(pit->bottom == pit->max_elements - 1)) {
			pit->bottom = 0;
		} else {
			pit->bottom++;
		}
		purged++;
	}
	return purged;
}


uint32_t pit_purge_expired_with_time(pit_t *pit, uint64_t *curr_time) {
	return __pit_purge_expired_with_time(pit, curr_time);
}


uint32_t pit_purge_expired(pit_t *pit) {
	uint64_t curr_time = get_curr_time();
	return __pit_purge_expired_with_time(pit, &curr_time);
}


void pit_free(pit_t *pit) {
	if(pit == NULL) {
		return;
	}
	if(pit->table != NULL) {
		rte_free(pit->table);
	}
	if(pit->ring != NULL) {
		rte_free(pit->ring);
	}
	rte_free(pit);
	return;
}
