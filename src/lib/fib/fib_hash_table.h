/*
 * Lorenzo Saino, Massimo Gallo
 *
 * Copyright (c) 2016 Alcatel-Lucent, Bell Labs
 *
 */

#ifndef _FIB_HASH_TABLE_H_
#define _FIB_HASH_TABLE_H_

/**
 * @file
 *
 * FIB hash table
 *
 * This file contains the implementation of the FIB hash table, which is a
 * linear open index hash table, whose implementation is explained in the paper:
 *
 * Diego Perino, Matteo Varvello, Leonardo Linguaglossa, Rafael Laufer, and
 * Roger Boislaigue, Caesar: a content router for high-speed forwarding on
 * content names. In Proc. of ACM/IEEE ANCS'14
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>

#include <rte_memory.h>

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
struct fib_htbl_entry {		// Size: 8 bytes
	uint32_t crc;		/**< CRC hash of the entry */
	uint32_t index;		/**< Index of the name entry in the forwarding table */
}  __attribute__((__packed__));

/**
 * A bucket of a linear open index hash table
 *
 * A bucket is basically an array of entries whose length is a cache line, i.e.
 * 64 bytes
 */
struct fib_htbl_bucket {	// Size: 64 bytes = 1 cache line
	uint8_t busy[BUCKET_SIZE];
	struct fib_htbl_entry entry[BUCKET_SIZE];
}  __attribute__((__packed__)) __rte_cache_aligned;

/**
 * Forwarding entry, mapping name to face ID
 *
 * A face ID is a 8-bit unsigned integer representing a "virtual face", i.e.
 * a next hop. This value needs then to be resolved to a physical port ID
 * and to the MAC address of the next hop to forward packets.
 */
struct fib_fwd_entry {	// This is equal to a line size (64 B)
	uint8_t face; 				 /**< index of next hop face */
	uint8_t name_len;			 /**< length of name in FIB entry */
	uint8_t name[MAX_NAME_LEN];  /**< name in FIB entry */
}__attribute__((__packed__)) __rte_cache_aligned;

/**
 * FIB hash table
 */
typedef struct {
	struct fib_htbl_bucket *htbl;    /**< array of buckets */
	struct fib_fwd_entry *fwd_table; /**< pointer to the forwarding table */
	uint32_t max_elements;		     /**< size of the FWD table */
	uint32_t num_buckets;		     /**< number of buckets in the hash table */
	uint32_t next_free_element;		 /**< index of the next free element in the FWD table */
} __attribute__((__packed__)) __rte_cache_aligned fibh_t;


/**
 * Return whether the FIB hash table is full or not
 *
 * @param fib_hash_table
 *   Pointer to the FIB hash table
 *
 * @return
 *   1 if full, 0 otherwise
 */
static inline
uint8_t is_fib_hash_table_full(fibh_t *fib_hash_table) {
	return fib_hash_table->next_free_element == fib_hash_table->max_elements;
}

/**
 * Return whether the FIB hash table is empty or not
 *
 * @param fib_hash_table
 *   Pointer to the FIB hash table
 *
 * @return
 *   1 if empty, 0 otherwise
 */
static inline
uint8_t is_fib_hash_table_empty(fibh_t *fib_hash_table) {
	return fib_hash_table->next_free_element == 0;
}

/**
 * Return number of items in the FIB
 *
 * @param fib_hash_table
 *   Pointer to the FIB hash table
 *
 * @return
 *   number of items
 */
static inline
uint32_t fib_hash_table_occupancy(fibh_t *fib_hash_table) {
	return fib_hash_table->next_free_element;
}

/**
 * Create a new FIB hash table
 *
 * @param num_buckets
 *   The number of buckets in the FIB hash table
 * @param max_elements
 *   Max number of elements supported, i.e. size of the circular log associated
 *   to the FIB hash table
 * @param socket
 *   ID of the NUMA socket on which the FIB will be created
 *
 * @return
 *   Pointer to the FIB hash table
 */
fibh_t* fib_hash_table_create(int num_buckets, int num_elements, int socket);

/**
 * Add a key to the FIB hash table
 *
 * @param fib_hash_table
 *   Pointer to the FIB hash table
 * @param name
 *   Name to insert
 * @param name_len
 *   Length of the name to insert
 * @param face
 *   The face ID associated to the entry
 *
 * @return
 * 	- 0 if the key was inserted successfully,
 * 	- -ENOSPC if the hash table is full
 */
int8_t fib_hash_table_add_key(fibh_t* fib_hash_table, uint8_t *name,
							uint8_t name_len, uint8_t face);

/**
 * Add a key to the FIB hash table, given its CRC32 hash
 *
 * @param fib_hash_table
 *   Pointer to the FIB hash table
 * @param name
 *   Name to insert
 * @param name_len
 *   Length of the name to insert
 * @param face
 *   The face ID associated to the entry
 * @param crc
 *   The CRC32 hash of the name
 *
 * @return
 * 	- 0 if the key was inserted successfully,
 * 	- -ENOSPC if the hash table is full
 */
int8_t fib_hash_table_add_key_with_hash(fibh_t* fib_hash_table, uint8_t *name,
							uint8_t name_len, uint8_t face, uint32_t crc);

/**
 * Lookup an entry in the hash table
 *
 * @param fib_hash_table
 *   Pointer to the FIB hash table
 * @param name
 *   Name to look up
 * @param name_len
 *   Length of the name to look up
 *
 * @return
 * 	- The face ID associated to key, if the key is present
 * 	- -ENOENT if the key is not found.
 */
int16_t fib_hash_table_lookup(fibh_t* fib_hash_table, uint8_t *name,
							uint8_t name_len);

/**
 * Lookup an entry in the hash table
 *
 * @param fib_hash_table
 *   Pointer to the FIB hash table
 * @param name
 *   Name to look up
 * @param name_len
 *   Length of the name to look up
 * @param crc
 *   The CRC32 hash of the name
 *
 * @return
 * 	- The face ID associated to key, if the key is present
 * 	- -ENOENT if the key is not found.
 */
int16_t fib_hash_table_lookup_with_hash(fibh_t* fib_hash_table, uint8_t *name,
							uint8_t name_len, uint32_t crc);

/**
 * Delete a key from the FIB hash table 
 * 
 * @param fib_hash_table
 *   Pointer to the FIB hash table
 * @param name
 *   Name to look up
 * @param name_len
 *   Length of the name to look up
 * @param face
 *   The face ID associated to the entry
 * 
 * @return
 * 	- 0 if the entry was deleted successfully
 * 	- -ENOENT if the key is not found.
 */
int8_t fib_hash_table_del_key(fibh_t* fib_hash_table, uint8_t *name,
							uint8_t name_len, uint8_t face);

/**
 * Delete a key from the FIB hash table, given its CRC32 hash 
 * 
 * @param fib_hash_table
 *   Pointer to the FIB hash table
 * @param name
 *   Name to look up
 * @param name_len
 *   Length of the name to look up
 * @param crc
 *   The CRC32 hash of the name
 * @param face
 *   The face ID associated to the entry
 * 
 * @return
 * 	- 0 if the entry was deleted successfully
 * 	- -ENOENT if the key is not found.
 */
int8_t fib_hash_table_del_key_with_hash(fibh_t* fib_hash_table, uint8_t *name,
							uint8_t name_len, uint32_t crc, uint8_t face);

/**
 * Free the memory used by the FIB hash table
 *
 * @param fib_hash_table
 *   Poitner to the FIB hash table
 */
void fib_hash_table_free(fibh_t* fib_hash_table);

#endif /* _FIB_HASH_TABLE_H_ */
