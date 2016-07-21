#include <string.h>
#include <stdio.h>

#include <rte_hash_crc.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_random.h>
#include <rte_branch_prediction.h>

#include <config.h>

#include "fib_hash_table.h"


fibh_t* fib_hash_table_create(int num_buckets, int max_elements, int socket) {

	fibh_t* htbl;
	void* p;
	// Allocate on the specified NUMA node. If socket is SOCKET_ID_ANY, then it
	// allocates the hash table on the socket of the calling lcore
	p = rte_zmalloc_socket("FIB_HASH_TABLE", sizeof(fibh_t),
			RTE_CACHE_LINE_SIZE, socket);
	if(p == NULL) {
		return NULL;
	}
	htbl = (fibh_t*) p;

	htbl->num_buckets = num_buckets;
	htbl->max_elements = max_elements;

	/* Allocate space for the actual hash-table */
	p = rte_zmalloc_socket("FIB_HASH_TABLE_BUCKETS",
			htbl->num_buckets*sizeof(struct fib_htbl_bucket),
			RTE_CACHE_LINE_SIZE, socket);
	if(p == NULL) {
		return NULL;
	}
	htbl->htbl = (struct fib_htbl_bucket*) p;

	/* Allocate space for the actual forwarding table */
	p = rte_zmalloc_socket("FIB_FWD_TABLE",
			htbl->max_elements*sizeof(struct fib_fwd_entry),
			RTE_CACHE_LINE_SIZE, socket);
	if(p == NULL) {
		return NULL;
	}
	htbl->fwd_table = (struct fib_fwd_entry *) p;

	return htbl;
}


static inline
int8_t __fib_hash_table_add_key_with_hash(fibh_t* fib_hash_table, uint8_t *name,
							uint8_t name_len, uint8_t face, uint32_t crc) {
	uint32_t bucket;
	uint8_t entry;
	if(unlikely(is_fib_hash_table_full(fib_hash_table))) {
		// Hash table is full
		return -ENOSPC;
	}

	/* Get index of corresponding bucket */
	bucket = crc % fib_hash_table->num_buckets;
	/* Iterate over all entries of the bucket till find one free and insert */
	for (entry = 0; entry < BUCKET_SIZE; entry++) {
		if (fib_hash_table->htbl[bucket].busy[entry] == 0) {

			fib_hash_table->htbl[bucket].busy[entry] = 1;
			fib_hash_table->htbl[bucket].entry[entry].crc = crc;
			fib_hash_table->htbl[bucket].entry[entry].index = fib_hash_table->next_free_element;
			fib_hash_table->fwd_table[fib_hash_table->next_free_element].face = face;
			fib_hash_table->fwd_table[fib_hash_table->next_free_element].name_len = name_len;
			rte_memcpy(fib_hash_table->fwd_table[fib_hash_table->next_free_element].name, name, name_len);
			fib_hash_table->next_free_element++;
			return 0;
		}
	}
	/*
	 * This line is reached if no free buckets are found and hence
	 * the insertion could not take place
	 */
	return -ENOSPC;
}


int8_t fib_hash_table_add_key_with_hash(fibh_t* fib_hash_table, uint8_t *name,
							uint8_t name_len, uint8_t face, uint32_t crc) {
	return __fib_hash_table_add_key_with_hash(fib_hash_table, name, name_len,
				face, crc);
}


int8_t fib_hash_table_add_key(fibh_t* fib_hash_table, uint8_t *name,
							uint8_t name_len, uint8_t face) {
	uint32_t crc = rte_hash_crc(name, name_len, MASTER_CRC_SEED);
	return __fib_hash_table_add_key_with_hash(fib_hash_table, name, name_len,
			face, crc);
}


static inline
int16_t __fib_hash_table_lookup_with_hash(fibh_t* fib_hash_table, uint8_t *name,
							uint8_t name_len, uint32_t crc) {
	uint32_t bucket;
	uint8_t entry;
	uint16_t match[APP_MAX_ETH_PORTS];
	uint16_t nmatch =0;
	uint16_t res;

	/* Get index of corresponding bucket */
	bucket = crc % fib_hash_table->num_buckets;
	/* Iterate all buckets till find one free and insert */
	for (entry = 0; entry < BUCKET_SIZE; entry++) {
		if (unlikely(fib_hash_table->htbl[bucket].busy[entry] == 0)) {
			/* Empty entry, unlikely because that would mean there is a whole*/
			continue;
		}
		if(unlikely(fib_hash_table->htbl[bucket].entry[entry].crc != crc)) {
			continue;
		}
		if(likely(name_len == fib_hash_table->fwd_table[fib_hash_table->htbl[bucket].entry[entry].index].name_len)) {
			if (memcmp(name, fib_hash_table->fwd_table[fib_hash_table->htbl[bucket].entry[entry].index].name, name_len) == 0) {
				match[nmatch++] = fib_hash_table->fwd_table[fib_hash_table->htbl[bucket].entry[entry].index].face;
			}
		}
	}
	if (nmatch == 0){
		/*
		* This line is reached if the searched element is not present in
		* the hash table
		*/
		return -ENOENT;
	}
	res = 0;
	if (nmatch >1) {
		res = rte_rand() % nmatch;
	}
	return match[res];
}


int16_t fib_hash_table_lookup(fibh_t* fib_hash_table, uint8_t *name,
							uint8_t name_len) {
	uint32_t crc = rte_hash_crc(name, name_len, MASTER_CRC_SEED);
	return __fib_hash_table_lookup_with_hash(fib_hash_table, name, name_len, crc);
}


int16_t fib_hash_table_lookup_with_hash(fibh_t* fib_hash_table, uint8_t *name,
							uint8_t name_len, uint32_t crc) {
	return __fib_hash_table_lookup_with_hash(fib_hash_table, name, name_len, crc);
}


static inline
int8_t __fib_hash_table_del_key_with_hash(fibh_t* fib_hash_table, uint8_t *name,
							uint8_t name_len, uint32_t crc, uint8_t face) {
	uint32_t bucket;
	uint8_t entry;
	/* Get index of corresponding bucket */
	bucket = crc % fib_hash_table->num_buckets;
	/* Iterate all buckets till find one free and insert */
	for (entry = 0; entry < BUCKET_SIZE; entry++) {
		if (fib_hash_table->htbl[bucket].busy[entry] == 0) {
			continue;
		}
		if(unlikely(fib_hash_table->htbl[bucket].entry[entry].crc != crc)) {
			continue;
		}
		
		if(likely(name_len == fib_hash_table->fwd_table[fib_hash_table->htbl[bucket].entry[entry].index].name_len &&
			memcmp(name, fib_hash_table->fwd_table[fib_hash_table->htbl[bucket].entry[entry].index].name, name_len) == 0 && 
			fib_hash_table->fwd_table[fib_hash_table->htbl[bucket].entry[entry].index].face == face)) {
		  
			fib_hash_table->htbl[bucket].busy[entry] = 0;
			return 0;
		}
	}
	/*
	 * This line is reached if the searched elements in not present in
	 * the hash table
	 */
	return -ENOENT;
}


int8_t fib_hash_table_del_key(fibh_t* fib_hash_table, uint8_t *name,
							uint8_t name_len, uint8_t face) {
	uint32_t crc = rte_hash_crc(name, name_len, MASTER_CRC_SEED);
	return __fib_hash_table_del_key_with_hash(fib_hash_table, name, name_len, crc, face);

}


int8_t fib_hash_table_del_key_with_hash(fibh_t* fib_hash_table, uint8_t *name,
							uint8_t name_len, uint32_t crc, uint8_t face) {
	return __fib_hash_table_del_key_with_hash(fib_hash_table, name, name_len, crc, face);
}


void fib_hash_table_free(fibh_t* fib_hash_table) {
	if(fib_hash_table == NULL) {
		return;
	}
	if(fib_hash_table->fwd_table != NULL) {
		rte_free(fib_hash_table->fwd_table);
	}
	if(fib_hash_table->htbl != NULL) {
		rte_free(fib_hash_table->htbl);
	}
	rte_free(fib_hash_table);
	return;
}



