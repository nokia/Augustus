/*
 * Lorenzo Saino, Massimo Gallo
 *
 * Copyright (c) 2016 Alcatel-Lucent, Bell Labs
 *
 */

#ifndef _PIT_H_
#define _PIT_H_

/**
 * @file
 *
 * Pending Interest Table (PIT)
 *
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
struct pit_table_entry {		// Size: 8 bytes
	uint32_t crc;		/**< CRC hash of the entry */
	uint32_t index;		/**< Index of the name entry in the forwarding table */
} __attribute__((__packed__));

/**
 * A bucket of a linear open index hash table
 *
 * A bucket is basically an array of entries whose length is a cache line, i.e.
 * 64 bytes
 */
struct pit_bucket {	// Size: 64 bytes = 1 cache line
	uint8_t busy[BUCKET_SIZE];
	struct pit_table_entry entry[BUCKET_SIZE];
} __attribute__((__packed__)) __rte_cache_aligned;

/**
 * Entry of the PIT
 *
 * This is one element of the PIT ring
 */
struct pit_entry {
	uint8_t active;				 /**< flag indicating whether this entry is used */
	uint32_t bucket;			 /**< bucket in the table, need this pointer for garbage collection */
	uint8_t tab;				 /**< tab in bucket, need this pointer for garbage collection */
	uint64_t expiry;			 /**< absolute expiration time in CPU cycles */
	uint8_t name_len;			 /**< length of name in PIT entry*/
	uint8_t name[MAX_NAME_LEN]; /**< name in PIT entry */
	uint64_t face_bitmask;		 /**< bitmask storing all faces from the Interest was received */
} __attribute__((__packed__)) __rte_cache_aligned;

/**
 * Pending Interest Table (PIT)
 */
typedef struct {
	struct pit_bucket *table;	/**< pointer to hash table */
	struct pit_entry *ring;		/**< pointer to ring of PIT entries */
	uint32_t max_elements;		/**< size of the PIT ring */
	uint32_t num_buckets;		/**< number of buckets in the hash table */
	uint32_t top;		 		/**< index of top (most recently inserted) entry */
	uint32_t bottom;			/**< index of bottom (least recently inserted) entry */
	uint64_t ttl;				/**< fixed TTL (in number of cycles) applied to all PIT entries */
} __attribute__((__packed__)) __rte_cache_aligned pit_t;

/**
 * Wrapper for DPDK function retrieving current time from CPU register
 *
 * @return
 *   current time, in number of cycles
 */
static inline
uint64_t get_curr_time() {
	return rte_rdtsc();
}

/**
 * The the PIT TTL in microseconds
 *
 * @param pit
 *   Pointer to the PIT
 *
 * @return
 *   The TTL, in microseconds
 */
static inline
uint64_t pit_get_ttl_us(pit_t *pit) {
	return (pit->ttl/rte_get_tsc_hz()) * US_PER_S;
}

/**
 * Set the TTL of the PIT
 *
 * This function is already called by the pit_create function. It is always
 * possible to change the TTL value during the PIT operation but bear that
 * in mind that an increase in TTL works correctly without transient issues,
 * a decrease may result in old (larger) TTL being applied to a number of
 * entries for a transient period
 *
 * @param pit
 *   Pointer to the PIT
 * @param ttl_us
 *   TTL in microseconds
 */
static inline
void pit_set_ttl_us(pit_t *pit, uint64_t ttl_us) {
	/* Note: this formula seems weird but it is correct, it's been verified */
	pit->ttl = ((rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S) * ttl_us;
}

/**
 * Return PIT occupancy, assuming there are no holes between head and tail
 * of the circular log
 *
 * @param pit
 *   pointer to the PIT
 *
 * @return
 *   PIT occupancy
 */
static inline
uint32_t pit_occupancy(pit_t *pit) {
	return (pit->top + pit->max_elements - pit->bottom) % pit->max_elements;
}

/**
 * Return whether the PIT is empty or not
 *
 * @param pit
 *   pointer to the PIT
 *
 * @return
 *   1 if the PIT is empty, 0 otherwise
 */
static inline
uint8_t is_pit_empty(pit_t *pit) {
	return pit->top == pit->bottom;
}

/**
 * Return whether the PIT is full or not
 *
 * @param pit
 *   pointer to the PIT
 *
 * @return
 *   1 if the PIT is full, 0 otherwise
 */
static inline
uint8_t is_pit_full(pit_t *pit) {
	return ((pit->top + 1) % pit->max_elements) == pit->bottom;
}

/**
 * Crate a PIT
 *
 * @param num_buckets
 *   The number of buckets in the PIT hash table
 * @param max_elements
 *   Max number of elements supported, i.e. size of the circular log associated
 *   to the PIT hash table
 * @param socket
 *   ID of the NUMA socket on which the PIT will be created
 *
 * @return
 *   Pointer to the PIT
 */
pit_t* pit_create(int num_buckets, int  max_elements, int socket, uint64_t ttl_us);

/**
 * Look up if an entry is in the PIT.
 *
 * This function is not meant to be used by the data plane, only by the control
 * plane. Data plane should use either pit_lookup_and_remove or
 * pit_lookup_and_update
 *
 * @param pit
 *   Pointer to the PIT
 * @param name
 *   Name of the chunk to look up
 * @param name_len
 *   Length of the chunk name to look up
 * @param crc
 *   CRC32 hash of the chunk name
 *
 * @return
 *  - Pointer to the PIT entry, if any
 *  - NULL, if entry is not in the PIT
 */
struct pit_entry *pit_lookup(pit_t *pit, uint8_t *name, uint8_t name_len,uint32_t crc);

/**
 * Lookup if an entry is in the PIT and, if so, remove it, given CRC32 hash
 *
 * This function is to be called when a Data packet arrive to do, in one single
 * call PIT lookup and removal.
 *
 * @param pit
 *   Pointer to the PIT
 * @param name
 *   Name of the chunk to look up
 * @param name_len
 *   Length of the chunk name to look up
 * @param crc
 *   CRC32 hash of the chunk name
 *
 * @return
 *   The facemask associated to the PIT entry, i.e. a bit mask where bit i is
 *   set to 1 if Data needs to be forwarded to face i. If the entry is not in
 *   the PIT, return 0
 */
uint64_t pit_lookup_and_remove_with_hash(pit_t *pit,
		uint8_t *name, uint8_t name_len, uint32_t crc);

/**
 * Lookup if an entry is in the PIT and, if so, remove it
 *
 * This function is to be called when a Data packet arrive to do, in one single
 * call PIT lookup and removal.
 *
 * @param pit
 *   Pointer to the PIT
 * @param name
 *   Name of the chunk to look up
 * @param name_len
 *   Length of the chunk name to look up
 *
 * @return
 *   The facemask associated to the PIT entry, i.e. a bit mask where bit i is
 *   set to 1 if Data needs to be forwarded to face i. If the entry is not in
 *   the PIT, return 0
 */
uint64_t pit_lookup_and_remove(pit_t *pit, uint8_t *name, uint8_t name_len);

/**
 * Look up if an entry is in the PIT and if not there, add it, given CRC32 hash
 *
 * This function is supposed to be called when an Interest packet arrives.
 *
 * @param pit
 *   Pointer to the PIT
 * @param name
 *   Name of the chunk to look up
 * @param name_len
 *   Length of the chunk name to look up
 * @param face
 *   Face from which Interest was received
 * @param curr_time
 *   Pointer to the current time in CPU cycles (used for expiration time calculation)
 * @param crc
 *   CRC32 hash of the name
 *
 * @return
 *  - 0 if the entry was already there 
 *  - 1 if the entry was not there and has been inserted by this function call
 *  - -ENOSPC if no space is available to insert the entry
 */
int8_t pit_lookup_and_update_with_hash(pit_t *pit,
		uint8_t *name, uint8_t name_len,
		uint8_t face, uint64_t *curr_time, uint32_t crc);

/**
 * Look up if an entry is in the PIT and if not there, add it
 *
 * This function is supposed to be called when an Interest packet arrives.
 *
 * @param pit
 *   Pointer to the PIT
 * @param name
 *   Name of the chunk to look up
 * @param name_len
 *   Length of the chunk name to look up
 * @param face
 *   Face from which Interest was received
 * @param curr_time
 *   Pointer to the current time in CPU cycles (used for expiration time calculation)
 *
 * @return
 *  - 0 if the entry was already there 
 *  - 1 if the entry was not there and has been inserted by this function call
 *  - -ENOSPC if no space is available to insert the entry
 */
int8_t pit_lookup_and_update(pit_t *pit, uint8_t *name, uint8_t name_len,
		uint8_t face, uint64_t *curr_time);

/**
 * Purge PIT of expired entries
 *
 * @param pit
 *   Pointer to the PIT
 *
 * @return
 *   Number of entries purged
 */
uint32_t pit_purge_expired(pit_t *pit);

/**
 * Purge PIT of expired entries, given a time
 *
 * @param pit
 *   Pointer to the PIT
 * @param curr_time
 *   Pointer to current timestamp in CPU cycles
 *
 * @return
 *   Number of entries purged
 */
uint32_t pit_purge_expired_with_time(pit_t *pit, uint64_t *curr_time);

/**
 * Free the memory allocated for the PIT
 *
 * @param pit
 *   Pointer to the PIT
 *
 */
void pit_free(pit_t *pit);

#endif /* _PIT_H_ */
