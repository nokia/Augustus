/*
 * Lorenzo Saino, Massimo Gallo
 *
 * Copyright (c) 2016 Alcatel-Lucent, Bell Labs
 *
 */

#ifndef _INIT_H_
#define _INIT_H_

/**
 * @file
 *
 * Initialization code
 */
#include <rte_common.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_byteorder.h>
#include <rte_eal.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ip.h>
#include <rte_mempool.h>
#include <rte_mbuf.h> //pkt_fwd_loop
#include <rte_hash_crc.h>
#include <rte_memory.h>
#include <rte_ether.h>

#include <fib/fib.h>
#include <pit/pit.h>
#include <cs/cs.h>

#include <config.h>

//#define MAX_RX_QUEUE_PER_LCORE 16
//#define MAX_TX_QUEUE_PER_PORT 16

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 128
#define RTE_TEST_TX_DESC_DEFAULT 512
static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;

/*
 * Static configuration of Ethernet device, used in main when calling for
 * each port "rte_eth_dev_configure"
 */
static const struct rte_eth_conf port_conf = {
	.rxmode = {
		.mq_mode = ETH_MQ_RX_RSS, // Enable RSS
		.split_hdr_size = 0,
		.max_rx_pkt_len = 8192, // set for Jumbo frame
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 1, /**< IP checksum offload disabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame    = 1, /**< Jumbo Frame Support disabled */
		.hw_strip_crc   = 0, /**< CRC stripped by hardware */
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = RSS_TOEPLITZ_KEY,
			.rss_key_len = 40,
			.rss_hf = RSS_HASH_FUNCTION,
		},
	},
};

// RX queues configuration, used later down in main
static const struct rte_eth_rxconf rx_conf = {
	.rx_thresh = {
		.pthresh = RX_PTHRESH,
		.hthresh = RX_HTHRESH,
		.wthresh = RX_WTHRESH,
	},
};

// TX queues configuration, used later down in main
static const struct rte_eth_txconf tx_conf = {
	.tx_thresh = {
		.pthresh = TX_PTHRESH,
		.hthresh = TX_HTHRESH,
		.wthresh = TX_WTHRESH,
	},
	.tx_free_thresh = 0, /* Use PMD default values */
	.tx_rs_thresh = 0,   /* Use PMD default values */
	/*
	* As the example won't handle mult-segments and offload cases,
	* set the flag by default.
	*/
	// Set to 0 for Jumbo frames
	.txq_flags = 0, //ETH_TXQ_FLAGS_NOMULTSEGS | ETH_TXQ_FLAGS_NOOFFLOADS,
};


/**
 * Structure tracking local and remote MAC addresses for each port
 */
struct port_addr {
	struct ether_addr local_addr;
	struct ether_addr remote_addr;
};


/** statistics collected by lcores for forwarding application */
struct stats {
	uint32_t int_recv; /**< number of Interest packets received */
	uint32_t int_cs_hit; /**< number of Interest packets served by CS */
	uint32_t int_pit_hit; /**< number of Interest packets aggregated in PIT (after CS miss) */
	uint32_t int_fib_hit; /**< number of Interest packets forwarded (after PIT miss) */
	uint32_t int_fib_loop;
	uint32_t int_no_route;
	uint32_t data_recv; /**< number of Data received */
	uint32_t data_sent;
	uint32_t data_pit_miss; /*< number of Data received for which no PIT entry is left */
	uint32_t nic_pkt_drop; /**< number of packet dropped in the NIC due to queue overflow */
	uint32_t sw_pkt_drop;	/**< number of packet dropped by SW data strucutre overflow */
	uint32_t malformed;		/**< number of malformed packets received */
}__attribute__((__packed__)) __rte_cache_aligned;


struct app_global_config {
	/* FIB settings */
	uint32_t fib_num_buckets;
	uint32_t fib_max_elements;
	uint32_t fib_bf_size;

	/* PIT settings */
	uint32_t pit_num_buckets;
	uint32_t pit_max_elements;
	uint32_t pit_ttl_us;

	/* CS settings */
	uint32_t cs_num_buckets;
	uint32_t cs_max_elements;

	/* Packet burst settings */
	uint16_t tx_burst_size;
	uint16_t rx_burst_size;

	/* Packet pool settings */
	uint32_t nb_mbuf;
	uint32_t mbuf_size;
	uint32_t mempool_cache_size;

	/* Other config */
	uint8_t promic_mode;
	uint32_t portmask;
	uint8_t numa_on;
	char    config_remote_addr[APP_MAX_ETH_PORTS][18];
	int sockfd;

}__attribute__((__packed__)) __rte_cache_aligned;


struct lcore_rx_queue {
	uint8_t port_id;
	uint8_t queue_id;
} __rte_cache_aligned;


struct app_lcore_config {
	/* packet buffers */
	struct rte_mempool *pktmbuf_pool;

	/* ports */
	uint8_t nb_rx_ports;
	uint8_t nb_ports;
	struct lcore_rx_queue rx_queue[APP_MAX_ETH_PORTS];
	uint16_t tx_queue_id[APP_MAX_ETH_PORTS];

	/* data structures */
	fib_t *fib;
	pit_t *pit;
	cs_t *cs;

	/* stats */
	struct stats stats;

	/* Port/MAC addr mappings */
	struct port_addr port_addr[APP_MAX_ETH_PORTS];

}__attribute__((__packed__)) __rte_cache_aligned;


/**
 * Initialize all configuration parameters of the application
 */
void init_app(struct app_global_config *app, struct app_lcore_config lcore[]);


#endif /* _INIT_H_ */
