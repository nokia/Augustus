/*
 * Lorenzo Saino, Massimo Gallo
 *
 * Copyright (c) 2016 Alcatel-Lucent, Bell Labs
 *
 */

#ifndef _CS_H_
#define _CS_H_

/**
 * @file
 *
 * Content Store (CS)
 *
 * The content store evicts items according to the the FIFO replacement policy
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>

#include <rte_memory.h>
#include <rte_cycles.h>

#include <config.h>

/**
 * Number of entries in a bucket
 *
 * This is sized to ensure that a bucket fits in a cache line of the x86
 * architecture (i.e. 64 bytes)
 */
#define BUCKET_SIZE	7

/**
 * A single entry of a linear open index hash table
 *
 * A bucket is an array of multiple of this entries up to fill a cache line,
 * plus, optionally a pointer to another element if overflowing.
 */
struct cs_table_entry {		// Size: 8 bytes
	uint32_t crc;		/**< CRC hash of the entry */
	uint32_t index;		/**< Index of the name entry in the forwarding table */
}  __attribute__((__packed__));


/**
 * A bucket of a linear open index hash table
 *
 * A bucket is basically an array of entries whose length is a cache line, i.e.
 * 64 bytes
 */
struct cs_bucket {	// Size: 64 bytes = 1 cache line
	uint8_t busy[BUCKET_SIZE];	/**< Array of byte-size entry indicating if corresponding entry is busy or not */
	struct cs_table_entry entry[BUCKET_SIZE];	/**< Hash-table entry */
}  __attribute__((__packed__)) __rte_cache_aligned;


/**
 * Entry of the CS
 *
 * This is one element of the CS ring
 */
struct cs_entry {
	uint8_t active;				 /**< flag indicating whether this entry is used */
	uint32_t bucket;			 /**< bucket in the table, need this pointer for garbage collection */
	uint8_t tab;				 /**< tab in bucket, need this pointer for eviction */
	uint8_t name_len;			 /**< length of name in CS entry*/
	uint8_t name[MAX_NAME_LEN]; /**< name in CS entry */
	struct rte_mbuf *mbuf;		/*< pointer to the RTE mbuf containing the packet */
}__attribute__((__packed__)) __rte_cache_aligned;


/**
 * Content Store (CS)
 */
typedef struct {
	struct cs_bucket *table;	/**< pointer to hash table */
	struct cs_entry *ring;		/**< pointer to ring of CS entries */
	uint32_t max_elements;		/**< size of the CS ring */
	uint32_t num_buckets;		/**< number of buckets in the hash table */
	uint32_t top;		 		/**< index of top (most recently inserted) entry */
	uint32_t bottom;			/**< index of bottom (least recently inserted) entry */
} __attribute__((__packed__)) __rte_cache_aligned cs_t;


/**
 * Return number of objects currently in the content store
 *
 * @param cs
 *   pointer to the CS
 *
 * @return
 *   number of items currently stored
 */
static inline
uint32_t cs_occupancy(cs_t *cs) {
	return (cs->top + cs->max_elements - cs->bottom) % cs->max_elements;
}

/**
 * Return whether the CS is empty or not
 *
 * @param cs
 *   pointer to the CS
 *
 * @return
 *   1 if the CS is empty, 0 otherwise
 */
static inline
uint8_t is_cs_empty(cs_t *cs) {
	return cs->top == cs->bottom;
}

/**
 * Return whether the CS is full or not
 *
 * @param cs
 *   pointer to the CS
 *
 * @return
 *   1 if the CS is full, 0 otherwise
 */
static inline
uint8_t is_cs_full(cs_t *cs) {
	return ((cs->top + 1) % cs->max_elements) == cs->bottom;
}

/**
 * Free the memory allocated for the CS
 *
 * @param cs
 *   pointer to the CS
 */
void cs_free(cs_t *cs);


/**
 * Create a Content Store (CS)
 *
 * @param num_buckets
 *   The number of buckets in the CS hash table
 * @param max_elements
 *   Max number of elements supported, i.e. size of the circular log associated
 *   to the CS hash table
 * @param socket
 *   ID of the NUMA socket on which the CS will be created
 *
 * @return
 *   pointer to the CS
 */
cs_t* cs_create(int num_buckets, int  max_elements, int socket);

/**
 * Insert a new chunk in the Content Store, given CRC32 hash of the chunk name.
 *
 * If the CS is full, evict an item according to FIFO policy.
 *
 * @param cs
 *   Pointer to the CS
 * @param name
 *   Name of the chunk to insert
 * @param name_len
 *   Length of the chunk name to insert
 * @param mbuf
 *   Pointer to the mbuf storing the Data packet to add to the CS
 * @param crc
 *   CRC32 hash of the chunk name
 *
 * @return
 *  - 0 if inserted correctly
 *  - -ENOSPC if the hash table bucket is full (very unlikely)
 *
 */
int8_t cs_insert_with_hash(cs_t *cs, uint8_t *name, uint8_t name_len, struct rte_mbuf *mbuf, uint32_t crc);

/**
 * Insert a new chunk in the Content Store
 *
 * If the CS is full, evict an item according to FIFO policy.
 *
 * @param cs
 *   Pointer to the CS
 * @param name
 *   Name of the chunk to insert
 * @param name_len
 *   Length of the chunk name to insert
 * @param mbuf
 *   Pointer to the mbuf storing the Data packet to add to the CS
 *
 * @return
 *  - 0 if inserted correctly
 *  - -ENOSPC if the hash table bucket is full (very unlikely)
 */
int8_t cs_insert(cs_t *cs, uint8_t *name, uint8_t name_len, struct rte_mbuf *mbuf);

/**
 * Lookup an item in cache, given CRC32 hash of the chunk name
 *
 * @param cs
 *   Pointer to the CS
 * @param name
 *   Name of the chunk to look up
 * @param name_len
 *   Length of the chunk name to look up
 * @param crc
 *   CRC32 hash of the chunk name
 *
 * @return
 *  - pointer to RTE mbuf containg the Data packet, in case of a hit
 *  - NULL, in case of a miss
 */
struct rte_mbuf *cs_lookup_with_hash(cs_t *cs, uint8_t *name, uint8_t name_len, uint32_t crc);

/**
 * Lookup an item in cache
 *
 * @param cs
 *   Pointer to the CS
 * @param name
 *   Name of the chunk to look up
 * @param name_len
 *   Length of the chunk name to look up
 *
 * @return
 *  - pointer to RTE mbuf in case of a hit
 *  - NULL  in case of a miss
 */
struct rte_mbuf *cs_lookup(cs_t *cs, uint8_t *name, uint8_t name_len);


#endif /* _CS_H_ */
