/*
 * Lorenzo Saino, Massimo Gallo
 *
 * Copyright (c) 2016 Alcatel-Lucent, Bell Labs
 *
 */
#ifndef _FIB_H_
#define _FIB_H_

/**
 * @file
 *
 * Forwarding Information Base (FIB)
 */

#include <string.h>
#include <stdio.h>

#include <rte_memory.h>
#include <rte_ether.h>
#include <rte_ip.h>

// #include "pbf.h"
#include "fib_hash_table.h"
#include <packet.h>



/**
 * FIB data type
 */
typedef struct {
	fibh_t* table;	/**< Pointer to FIB hash table */
} __attribute__((__packed__)) __rte_cache_aligned fib_t;

/**
 * Create and initialize the FIB.
 * 
 * @param num_buckets
 *   Number of HT lines used to store the items.
 * @param max_elements
 *   Max numbers of items to be stored
 * @param bf_size
 *   Size of the Bloom filter (in bytes)
 * @param socket
 *   ID of the NUMA socket on which the FIB will be created
 *
 * @return
 *   Pointer to the FIB
 */
fib_t* fib_create(uint32_t num_buckets, uint32_t max_elements, uint32_t bf_size, int socket);

/**
 * Add a new entry to the FIB
 *
 * @param fib
 *   Pointer to the FIB
 * @param name
 *   Pointer to the name prefix to insert
 * @param name_len
 *   Length of the name prefix
 * @param face
 *   ID of the face associated to the name
 *
 * @return
 *  - 0 if entry is inserted successfully
 *  - -ENOSPCS if hash table is full
 *  - -ENIVAL if arguments are invalid, e.g. name == 0 or name == "\0"
 */
int8_t fib_add(fib_t *fib, uint8_t *name, uint16_t name_len, uint8_t face);

/**
 * Delete an entry from the FIB
 *
 * @param fib
 *   Pointer to the FIB
 * @param name
 *   Pointer to the name prefix to insert
 * @param name_len
 *   Length of the name prefix
 * @param face
 *   ID of the face associated to the name
 *
 * @return
 *  - 0 if the entry was deleted successfully
 *  - -ENOENT if the key is not found.
 */
int8_t fib_del(fib_t *fib, uint8_t *name, uint16_t name_len, uint8_t face);

/**
 * Look up an entry into the FIB
 *
 * @param fib
 *   Pointer to the FIB
 * @param icn_packet
 *   Pointer to the structure storing the parsed packet
 *
 * @return
 *  - face ID associated to the entry, if present (random if more than one)
 *  - -ENOENT if the queried name is not in the FIB
 */
int8_t fib_lookup(fib_t *fib, struct icn_packet * icn_packet);

/**
 * Free the memory used by the FIB and associated data structures
 *
 * @param fib
 *   The pointer to the FIB
 */
void fib_free(fib_t *fib);

#endif /* _FIB_H_ */
