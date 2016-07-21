/*
 * Lorenzo Saino, Massimo Gallo
 *
 * Copyright (c) 2016 Alcatel-Lucent, Bell Labs
 *
 */

#ifndef _DEFAULTS_H_
#define _DEFAULTS_H_

/**
 * @file
 *
 * Default configuration parameters
 *
 * This file contains the default configuration parameters of the DPDK content
 * router. These parameters can be overridden in the config.h file.
 */

/**************** Software information ****************/
#define AUGUSTUS_VERSION "0.1"

/**
 * Define it for high speed experiments, remove it for debug
 */
#define SUPPRESS_LOG

/**************** Name properties ****************/

/**
 * This name size is set so as to make a PIT entry fit in one cache line
 * (64 bytes)
 *
 * It's admittedly a bit short but can be extended by trimming something from
 * the PIT entry or by allowing PIT entry to fit in 2 cache lines and
 * possibly use some prefetching
 */
#define MAX_NAME_LEN 33

/**
 * Max number of components of a name
 */
#define MAX_NAME_COMPONENTS 16

/**
 * Component separator for names.
 *
 * Note, in defining the separator, use single quote, as it is supposed to be a
 * character, not a literal. A double quote will turn it into a literal and
 * the code won't work.
 */
#define COMPONENT_SEP (uint8_t) '/'

/**
 * Command separator for control plane commands.
 *
 * Note, in defining the separator, use single quote, as it is supposed to be a
 * character, not a literal. A double quote will turn it into a literal and
 * the code won't work.
 */
#define COMMAND_SEP (uint8_t) ':'

/**************** General machine capabilities ****************/

/**
 * Max number of cores available on the machine.
 * This is used for data structure optimization, the actual number of cores
 * used is specified at runtime by passing the COREMASK command line parameter
 */
#define APP_MAX_LCORES 16

/**
 * Core dedicated to the control plane.
 */
#define CONTROL_PLANE_LCORE 0

/**
 * Max number of NUMA cores on the system
 */
#define APP_MAX_SOCKETS 2

/**
 * Max number of Ethernet ports available on the system
 */
#define APP_MAX_ETH_PORTS 10


/**************** NIC capabilities and configuration ****************/

/* RSS config */

/**
 * Hash initiation key of the Toeplitz algorithm used by the RSS function of
 * the NICs.
 *
 * This key, which is 40 bytes long, is used to redirect incoming packets to
 * a specific hardware queue based on the 5-tuple of the packet. The following
 * key is designed such that hash results only depend on source IPv4 address
 * of the packet, which, in our implementation contains the CRC32 hash of the
 * content name.
 */
#define RSS_TOEPLITZ_KEY (uint8_t[]) { \
		0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, \
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  \
						 	 	 }
/**
 * Specifies that RSS hash should be computed only on IPv4 src/dst addresses
 */
#define RSS_HASH_FUNCTION ETH_RSS_IPV4

/*
 * RX and TX Prefetch, Host, and Write-back threshold values should be
 * carefully set for optimal performance. Consult the network
 * controller's datasheet and supporting DPDK documentation for guidance
 * on how these parameters should be set.
 */
#define RX_PTHRESH 8 /**< Default values of RX prefetch threshold reg. */
#define RX_HTHRESH 8 /**< Default values of RX host threshold reg. */
#define RX_WTHRESH 4 /**< Default values of RX write-back threshold reg. */

/*
 * These default values are optimized for use with the Intel(R) 82599 10 GbE
 * Controller and the DPDK ixgbe PMD. Consider using other values for other
 * network controllers and/or network drivers.
 */
#define TX_PTHRESH 36 /**< Default values of TX prefetch threshold reg. */
#define TX_HTHRESH 0  /**< Default values of TX host threshold reg. */
#define TX_WTHRESH 0  /**< Default values of TX write-back threshold reg. */

/**
 * Size of an mbuf (i.e. the data structure that containing a packet received
 * from the NIC)
 *
 * Note: Headroom is a free space at the beginning of the packet buffer that is
 * reserved to prepend data to a packet being processed. DPDK headroom value is
 * 128 bytes. We do not need any headroom because we do not need to prepend
 * data. To remove it we need to compile DPDK with setting
 * CONFIG_RTE_PKTMBUF_HEADROOM=0
 *
 */
#define MBUF_SIZE (2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)

/**
 * Number of packet buffers per NUMA socket
 */
#define NB_MBUF 8192

/**
 * Per-core cache size of packet mempool
 */
#define MEMPOOL_CACHE_SIZE 256


/******************* Data plane configuration **********************/

/* Data structure dimensioning */

/* FIB */
// Each value is per NUMA socket
// Keep FIB size small, don't really need a big one unless you are testing FIB
#define FIB_NUM_BUCKETS     10
#define FIB_MAX_ELEMENTS    20

/* PIT */
// Each value is per core
#define PIT_NUM_BUCKETS     1024
#define PIT_MAX_ELEMENTS    8192

/* CS */
// Each value is per core
#define CS_NUM_BUCKETS      1024
#define CS_MAX_ELEMENTS     4096


/**
 * Max size of burst transmitted to be sent to a TX port in a batch
 */
#define MAX_PKT_BURST 32

/**
 * Transmission buffer drain period, in microseconds
 *
 * If not enough packets have been received to make a match of MAX_PKT_BURST
 * to forward and BURST_TX_DRAIN_US microseconds have passed since a packet
 * has been received, forward it anyway.
 */
#define BURST_TX_DRAIN_US 100

/**
 * Number of packets ahead to prefetch, when reading received packets
 */
#define PREFETCH_OFFSET	3

/**
 * Max period between subsequent PIT purges, in microseconds
 */
#define PIT_PURGE_US 20000000

/**
 * TTL of PIT entries, in microseconds
 */
#define PIT_TTL_US 5000000

/******************* Hash config ***************************/

/**
 * Seeds for calculating various hash values (used by FIB Prefix Bloom Filter)
 */
#define CRC_SEED (uint32_t[]) { \
		0x11111111, 0x22222222, 0x33333333, 0x44444444, 0x55555555, \
		0x66666666, 0x77777777, 0x88888888, 0x99999999, 0xAAAAAAAA, \
		0xBBBBBBBB, 0xCCCCCCCC, 0xDDDDDDDD, 0xEEEEEEEE, 0xFFFFFFFF  \
							  }

/**
 * Main CRC hash seed
 */
#define MASTER_CRC_SEED (uint32_t) CRC_SEED[0]


#endif /* _DEFAULTS_H_ */
