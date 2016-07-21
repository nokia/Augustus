/*
 * Lorenzo Saino, Massimo Gallo
 *
 * Copyright (c) 2016 Alcatel-Lucent, Bell Labs
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdarg.h>
#include <errno.h>
#include <inttypes.h>

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
#include <rte_mbuf.h>
#include <rte_hash_crc.h>

#include <config.h>
#include <packet.h>
#include <fib/fib.h>
#include <pit/pit.h>
#include <cs/cs.h>

#include "data_plane.h"
#include "init.h"
#include "util.h"


#define DATA_PLANE_LOG(...) printf("[DATA PLANE]: " __VA_ARGS__)

/* 
 * LOGS suppression. Remove if need to debug.
 */
#ifdef SUPPRESS_LOG
#ifdef RTE_LOG
#undef RTE_LOG
#define RTE_LOG(l, t, ...)
#endif
#endif


struct mbuf_table {
	uint16_t len;
	struct rte_mbuf *m_table[MAX_PKT_BURST];
	uint8_t keep[MAX_PKT_BURST];
} __rte_cache_aligned;


void reset_stats() {
	uint8_t lcore_id, nb_lcores;
	nb_lcores = get_nb_lcores_available();
	for(lcore_id = 0; lcore_id < nb_lcores; lcore_id++) {
		lcore_conf[lcore_id].stats.int_recv = 0;
		lcore_conf[lcore_id].stats.int_cs_hit = 0;
		lcore_conf[lcore_id].stats.int_pit_hit = 0;
		lcore_conf[lcore_id].stats.int_fib_hit = 0;
		lcore_conf[lcore_id].stats.int_fib_loop = 0;
		lcore_conf[lcore_id].stats.int_no_route = 0;
		lcore_conf[lcore_id].stats.data_recv = 0;
		lcore_conf[lcore_id].stats.data_sent = 0;
		lcore_conf[lcore_id].stats.data_pit_miss = 0;
		lcore_conf[lcore_id].stats.nic_pkt_drop = 0;
		lcore_conf[lcore_id].stats.sw_pkt_drop = 0;
		lcore_conf[lcore_id].stats.malformed = 0;
	}
}


void print_stats() {
	uint8_t lcore_id, nb_lcores;
	nb_lcores = get_nb_lcores_available();
	struct stats global_stats;
	/* Init global stats */
	global_stats.int_recv = 0;
	global_stats.int_cs_hit = 0;
	global_stats.int_pit_hit = 0;
	global_stats.int_fib_hit = 0;
	global_stats.int_fib_loop = 0;
	global_stats.int_no_route = 0;
	global_stats.data_recv = 0;
	global_stats.data_sent = 0;
	global_stats.data_pit_miss = 0;
	global_stats.nic_pkt_drop = 0;
	global_stats.sw_pkt_drop = 0;
	global_stats.malformed = 0;
	printf("Statistics:\n");
	for(lcore_id = 0; lcore_id < nb_lcores; lcore_id++) {
		if(!rte_lcore_is_enabled(lcore_id)) {
			continue;
		}
		printf("  [LCORE %u]:\n", lcore_id);
		printf("    Interest recv: %u\n", lcore_conf[lcore_id].stats.int_recv);
		printf("    CS hits: %u\n", lcore_conf[lcore_id].stats.int_cs_hit);
		printf("    PIT hits: %u\n", lcore_conf[lcore_id].stats.int_pit_hit);
		printf("    FIB hits: %u\n", lcore_conf[lcore_id].stats.int_fib_hit);
		printf("    FIB loop: %u\n", lcore_conf[lcore_id].stats.int_fib_loop);
		printf("    Interest no route: %u\n", lcore_conf[lcore_id].stats.int_no_route);
		printf("    Data received: %u\n", lcore_conf[lcore_id].stats.data_recv);
		printf("    Data sent: %u\n", lcore_conf[lcore_id].stats.data_sent);
		printf("    Data PIT miss: %u\n", lcore_conf[lcore_id].stats.data_pit_miss);
		printf("    Packet drops (NIC): %u\n", lcore_conf[lcore_id].stats.nic_pkt_drop);
		printf("    Packet drops (SW): %u\n", lcore_conf[lcore_id].stats.sw_pkt_drop);
		printf("    Malformed: %u\n", lcore_conf[lcore_id].stats.malformed);
		global_stats.int_recv += lcore_conf[lcore_id].stats.int_recv;
		global_stats.int_cs_hit += lcore_conf[lcore_id].stats.int_cs_hit;
		global_stats.int_pit_hit += lcore_conf[lcore_id].stats.int_pit_hit;
		global_stats.int_fib_hit += lcore_conf[lcore_id].stats.int_fib_hit;
		global_stats.int_fib_loop += lcore_conf[lcore_id].stats.int_fib_loop;
		global_stats.int_no_route += lcore_conf[lcore_id].stats.int_no_route;
		global_stats.data_recv += lcore_conf[lcore_id].stats.data_recv;
		global_stats.data_sent += lcore_conf[lcore_id].stats.data_sent;
		global_stats.data_pit_miss += lcore_conf[lcore_id].stats.data_pit_miss;
		global_stats.nic_pkt_drop += lcore_conf[lcore_id].stats.nic_pkt_drop;
		global_stats.sw_pkt_drop += lcore_conf[lcore_id].stats.sw_pkt_drop;
		global_stats.malformed += lcore_conf[lcore_id].stats.malformed;
	}
	printf("  [GLOBAL]:\n");
	printf("    Interest recv: %u\n", global_stats.int_recv);
	printf("    CS hits: %u\n", global_stats.int_cs_hit);
	printf("    PIT hits: %u\n", global_stats.int_pit_hit);
	printf("    FIB hits: %u\n", global_stats.int_fib_hit);
	printf("    FIB loop: %u\n", global_stats.int_fib_loop);
	printf("    Interest no route: %u\n", global_stats.int_no_route);
	printf("    Data received: %u\n", global_stats.data_recv);
	printf("    Data sent: %u\n", global_stats.data_sent);
	printf("    Data PIT miss: %u\n", global_stats.data_pit_miss);
	printf("    Packet drops (NIC): %u\n", global_stats.nic_pkt_drop);
	printf("    Packet drops (SW): %u\n", global_stats.sw_pkt_drop);
	printf("    Malformed: %u\n", global_stats.malformed);
	printf("=== END ===\n");
}


/* Send the burst of packets on an output interface */
static int
send_burst(struct mbuf_table *tx_mbuf, uint32_t n,
		uint8_t tx_port, uint8_t tx_queue, struct stats *stats) {
	struct rte_mbuf **m_table;
	uint32_t ret;
	m_table = tx_mbuf->m_table;
	ret = rte_eth_tx_burst(tx_port, (uint16_t) tx_queue, m_table, (uint16_t) n);
	RTE_LOG(DEBUG, AUGUSTUS, "LCORE_%u: Sent burst of %u packets to (port=%u, queue=%u)\n",
			rte_lcore_id(), n, tx_port, tx_queue);
	if (unlikely(ret < n)) {
		stats->nic_pkt_drop += (n - ret);
		do {
			/* Here we free the buffer of the packets that have not been sent
			 * due to queue overflow in the NIC
			 */
			rte_pktmbuf_free(m_table[ret]);
		} while (++ret < n);
	}
	return 0;
}


/*
 * Enqueue a packet for TX and if the number of packets currently enqueued on
 * the given destination port is equal to a MAX_PKT_BURST constant, then it
 * sends the entire burst, otherwise it just enqueues the packet
 */
static int
send_single_packet(struct rte_mbuf *m, struct mbuf_table *tx_mbuf,
		uint8_t tx_port, uint8_t tx_queue, struct stats *stats)
{
	uint16_t len;
	len = tx_mbuf->len;
	tx_mbuf->m_table[len] = m;
	len++;

	RTE_LOG(DEBUG, AUGUSTUS, "LCORE_%u: Enqueued pkt %u for tx on port %u\n",
			rte_lcore_id(), len, tx_port);
	/* Enough pkts to be sent, send burst */
	if (unlikely(len == MAX_PKT_BURST)) {
		send_burst(tx_mbuf, MAX_PKT_BURST, tx_port, tx_queue, stats);
		len = 0;
	}
	tx_mbuf->len = len;
	return 0;
}


static void
icn_fwd(struct rte_mbuf *m, uint8_t rx_port_id,  struct app_lcore_config *conf,
		struct mbuf_table tx_mbufs[]) {

	/* Pointers to headers of the processed packet */
	struct ether_hdr 	*eth_hdr;
	struct ipv4_hdr  	*ipv4_hdr;
	uint8_t          	*pkt;           /*points to the beginning of the icn packet*/
	struct icn_packet	icn_pkt;

	int8_t ret;
	uint32_t crc, portmask;
	struct rte_mbuf *data; // pointer to the data packet in cache (if hit)

	/* ID of destination port, which will be resolved after FIB lookup */
	uint8_t tx_port_id, tx_queue_id;

	/* Cast the head of the packet buffer to an Ethernet header */
	eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);

	/*
	 * If the received packet is not an IPv4 one (e.g. ARP and ICMP), drop it
	 * without processing to save resources. In performance testing scenarios
	 * packet routing will be done with VLANs statically configured in an
	 * Ethernet switch
	 *
	 */
	if(unlikely(eth_hdr->ether_type != ETHER_TYPE_IPv4_BE)) {
		RTE_LOG(DEBUG, AUGUSTUS, "LCORE_%u: Received non-IPv4 packet "
				"from port %u. Dropping\n", rte_lcore_id(), rx_port_id);
		rte_pktmbuf_free(m);
		conf->stats.malformed++;
		return;
	}

	ipv4_hdr = (struct ipv4_hdr *)RTE_PTR_ADD(eth_hdr, sizeof(struct ether_hdr));

	if(unlikely(ipv4_hdr->next_proto_id != IPPROTO_ICN)) {
		RTE_LOG(DEBUG, AUGUSTUS, "LCORE_%u: Received IPv4 packet w/o ICN "
				"payload from port %u. Dropping\n", rte_lcore_id(), rx_port_id);
		rte_pktmbuf_free(m);
		conf->stats.malformed++;
		return;
	}
	
	pkt = (uint8_t*) RTE_PTR_ADD(ipv4_hdr, sizeof(struct ipv4_hdr));
	parse_packet(pkt, &icn_pkt);

	/*
	 * Calculate CRC32 hash 
	 */
	crc = rte_hash_crc(icn_pkt.name, icn_pkt.name_len, MASTER_CRC_SEED);
	
	icn_pkt.crc[icn_pkt.component_nr] = crc;

	if (icn_pkt.hdr->type == TYPE_INTEREST_BE) {
		RTE_LOG(DEBUG, AUGUSTUS, "LCORE_%u: Received Interest for '%.*s' from port %u. "
				"Processing\n", rte_lcore_id(), icn_pkt.name_len, icn_pkt.name, rx_port_id);
		conf->stats.int_recv++;
		/* Lookup in CS */
		data = cs_lookup_with_hash(conf->cs, icn_pkt.name, icn_pkt.name_len, crc);
		if(data != NULL) {
 			RTE_LOG(DEBUG, AUGUSTUS, "LCORE_%u: CS hit for '%.*s'\n",
 					rte_lcore_id(), icn_pkt.name_len, icn_pkt.name);
			/* CS hit: Reply and delete interest */
			conf->stats.int_cs_hit++;
			/* Increase reference counter by 1 otherwise sending the Data
			 * packet will also delete it
			 */
			rte_pktmbuf_refcnt_update(data, 1);
			/* Only edit src and dst MAC address and assume that all other
			 * fields have been verified at CS insertion
			 */
			eth_hdr = rte_pktmbuf_mtod(data, struct ether_hdr *);
			ether_addr_copy(&conf->port_addr[rx_port_id].local_addr, &eth_hdr->s_addr);
			ether_addr_copy(&conf->port_addr[rx_port_id].remote_addr, &eth_hdr->d_addr);

			send_single_packet(data, &(tx_mbufs[rx_port_id]), rx_port_id,
					conf->tx_queue_id[rx_port_id], &(conf->stats));
			conf->stats.data_sent++;
			/* Drop received interest*/
			rte_pktmbuf_free(m);
			return;
		} else { /* CS miss */
			/* check PIT */
 			RTE_LOG(DEBUG, AUGUSTUS, "LCORE_%u: CS miss for '%.*s'\n",
 					rte_lcore_id(), icn_pkt.name_len, icn_pkt.name);
 			ret = pit_lookup_and_update_with_hash(conf->pit, icn_pkt.name, icn_pkt.name_len, rx_port_id, NULL, crc);
			if(unlikely(ret != 1)) {	/* PIT aggregation or PIT bucket overflow */
				/*
				 * Reach this in case the entry was already in the PIT and
				 * got aggregated or could not insert it due to lack of space
				 * In any case, I don't forward the Interest. However in the
				 * case of no space I print a debug message.
				 *
				 */
				if(unlikely(ret == -ENOSPC)) {	/* PIT bucket overflow */
					RTE_LOG(DEBUG, AUGUSTUS, "LCORE_%u: Could not insert "
							"Interest in PIT because full or bucket overflow. "
							"Dropping\n", rte_lcore_id());
					conf->stats.sw_pkt_drop++;
				} else {	/* PIT aggregation */
					conf->stats.int_pit_hit++;
 					RTE_LOG(DEBUG, AUGUSTUS, "LCORE_%u: PIT aggregation for '%.*s'\n",
 							rte_lcore_id(), icn_pkt.name_len, icn_pkt.name);
				}
				rte_pktmbuf_free(m);
			} else {	/* CS and PIT miss */
				/* query FIB and forward */
				RTE_LOG(DEBUG, AUGUSTUS, "LCORE_%u: PIT miss for '%.*s'\n",
											rte_lcore_id(), icn_pkt.name_len, icn_pkt.name);
				
				ret = fib_lookup(conf->fib, &icn_pkt);
				RTE_LOG(DEBUG, AUGUSTUS, "LCORE_%u: FIB forwarding for '%.*s' to face %d\n",
						rte_lcore_id(), icn_pkt.name_len, icn_pkt.name, ret);
				if(unlikely(ret < 0)) {
					RTE_LOG(DEBUG, AUGUSTUS, "LCORE_%u: No FIB entry for name "
							"'(%.*s)'. Dropping packet\n",
							rte_lcore_id(), icn_pkt.name_len, icn_pkt.name);
					conf->stats.int_no_route++;
					pit_lookup_and_remove_with_hash(conf->pit, icn_pkt.name, icn_pkt.name_len, crc);
					rte_pktmbuf_free(m);
					return;
				} else if(unlikely(ret == rx_port_id)) {
					/* Packet come from direction is supposed to go to: loop */
					RTE_LOG(DEBUG, AUGUSTUS, "LCORE_%u: FIB entry for name "
							"'(%.*s)' points to RX port. Dropping packet\n",
							rte_lcore_id(), icn_pkt.name_len, icn_pkt.name);
					conf->stats.int_fib_loop += 1;
					pit_lookup_and_remove_with_hash(conf->pit, icn_pkt.name, icn_pkt.name_len, crc);
					rte_pktmbuf_free(m);
					return;
				}
				conf->stats.int_fib_hit++;
				ether_addr_copy(&conf->port_addr[ret].local_addr, &eth_hdr->s_addr);
				ether_addr_copy(&conf->port_addr[ret].remote_addr, &eth_hdr->d_addr);
				send_single_packet(m, &(tx_mbufs[ret]), ret,
						conf->tx_queue_id[ret], &(conf->stats));
			}
		}
	} else if (icn_pkt.hdr->type == TYPE_DATA_BE) {
		RTE_LOG(DEBUG, AUGUSTUS, "LCORE_%u: Received Data for '%.*s' from port %u.\n",
				rte_lcore_id(), icn_pkt.name_len, icn_pkt.name, rx_port_id);
		/* First insert it in CS. Note: for performance reasons, the CS
		 * implementation assumes that the content is not the CS when an insert
		 * operation is attempted and it therefore does not check if it actually
		 * is not in the CS. If there is already a copy in the cache, a
		 * duplicated will be stored
		 */
		conf->stats.data_recv += 1;
		ret = cs_insert_with_hash(conf->cs, icn_pkt.name, icn_pkt.name_len, m, crc);
		portmask = pit_lookup_and_remove_with_hash(conf->pit, icn_pkt.name, icn_pkt.name_len,crc);
		if(unlikely(portmask == 0)) {
			/* Probably it expired in the PIT. Quit without freeing the mbuf
			 * because the packet is in the CS */
			RTE_LOG(DEBUG, AUGUSTUS, "LCORE_%u: No PIT entry for Data '%.*s' from port %u. Dropping\n",
							rte_lcore_id(), icn_pkt.name_len, icn_pkt.name, rx_port_id);
			conf->stats.data_pit_miss++;
			return;
		}
		/*
		 * If the packet is also sent back (which is the normal case when
		 * there is a valid PIT entry) increase its reference count for each
		 * port in the portmask otherwise the mbuf is freed and the CS will
		 * have a dangling pointer
		 *
		 * Iterate over the portmask to find ports to which Data is to be sent
		 */
		for(tx_port_id = 0; tx_port_id < APP_MAX_ETH_PORTS; tx_port_id++) {
			if((portmask & 1) == 1) {
				rte_pktmbuf_refcnt_update(m, 1);

				ether_addr_copy(&conf->port_addr[tx_port_id].local_addr, &eth_hdr->s_addr);
				ether_addr_copy(&conf->port_addr[tx_port_id].remote_addr, &eth_hdr->d_addr);
				RTE_LOG(DEBUG, AUGUSTUS, "LCORE_%u: Forwarding Data for '%.*s' to port %u\n",
											rte_lcore_id(), icn_pkt.name_len, icn_pkt.name, tx_port_id);
				send_single_packet(m, &(tx_mbufs[tx_port_id]), tx_port_id,
						conf->tx_queue_id[tx_port_id], &(conf->stats));
				conf->stats.data_sent++;
			}
			portmask >>= 1;
		}
		return;
	} else {
		RTE_LOG(DEBUG, AUGUSTUS, "LCORE_%u: Received malformed ICN packet "
				"from port %u. Dropping\n", rte_lcore_id(), rx_port_id);
		conf->stats.malformed++;
		rte_pktmbuf_free(m);
		return;
	}
	/* This block of code should never be reached*/
	return;
}


/* Main data plane processing loop */
int pkt_fwd_loop(__attribute__((unused)) void *arg) {
	struct app_lcore_config *conf;
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct rte_mbuf *m;
	unsigned lcore_id, socket_id;
	uint64_t prev_drain_tsc, prev_pit_purge_tsc, cur_tsc;
	int i, j, nb_rx;
	uint8_t port_id, queue_id;

	/* Max number of cycle allowed between subsequent packet transmission
	 * Since packets are batched for transmission, if load is low a packet
	 * may wait for too long to be transmitted. This variable is the maximum
	 * delay a packet can wait in the batching queue. If a batch is not
	 * complete within this period, the packets are sent out anyway.
	 */
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) /
			US_PER_S * (uint64_t) BURST_TX_DRAIN_US;
	const uint64_t pit_purge_tsc = (rte_get_tsc_hz() + US_PER_S - 1) /
			US_PER_S * (uint64_t) PIT_PURGE_US;

	/* Used to pass pointers to packets to send. Init it */
	struct mbuf_table tx_mbufs[APP_MAX_ETH_PORTS];
	for(port_id = 0; port_id < APP_MAX_ETH_PORTS; port_id++) {
		tx_mbufs[port_id].len = 0;
	}

	/*
	 * Reset TSC counters before entering the main loop
	 */
	prev_drain_tsc = 0;
	prev_pit_purge_tsc = 0;
	cur_tsc = 0;

	/* get ID of the lcore on which it is running */
	lcore_id = rte_lcore_id();
	socket_id = rte_socket_id();

	/* Get core configuration */
	conf = &lcore_conf[lcore_id];

	DATA_PLANE_LOG("[LCORE_%u] Started\n", lcore_id);

	/* The core has no RX queues to listen from */
	if (conf->nb_rx_ports == 0) {
		DATA_PLANE_LOG("[LCORE_%u] I have no RX queues to read from. I quit\n", lcore_id);
		return -1;
	}

	for (i = 0; i < conf->nb_rx_ports; i++) {
		port_id = conf->rx_queue[i].port_id;
		queue_id = conf->rx_queue[i].queue_id;
		DATA_PLANE_LOG("[LCORE_%u] Listening on (port_id=%u, queue_id=%u)\n",
				lcore_id, port_id, queue_id);
	}

	while (1) {
		/* Get current CPU cycle number */
		cur_tsc = rte_rdtsc();

		/* TX burst queue drain */
		if (unlikely((cur_tsc - prev_drain_tsc) > drain_tsc)) {
			/*
			 * If the previous polling cycle took longer than a certain time,
			 * this block of code transmits all packets enqueued right now
			 * (this is to upper bound tx batching latency).
			 * After this isfile done, continue and receive packets. This iteration
			 * occurs over all TX queues allocated to this lcore.
			 *
			 * This is also a good time to purge the PIT because if I reach
			 * this block, that means that the router is not loaded
			 */
			prev_drain_tsc = cur_tsc;
			for (port_id = 0; port_id < APP_MAX_ETH_PORTS; port_id++) {

				if (tx_mbufs[port_id].len == 0) {
					continue;
				}
				send_burst(&tx_mbufs[port_id], tx_mbufs[port_id].len,
						 port_id, conf->tx_queue_id[port_id], &(conf->stats));
				tx_mbufs[port_id].len = 0;
			}
			/* purge PIT because in period of low load */
			pit_purge_expired_with_time(conf->pit, &cur_tsc);
			prev_pit_purge_tsc = cur_tsc;
		}
		/* purge PIT because max inter-purge period reached */
		if(unlikely((cur_tsc - prev_pit_purge_tsc) > pit_purge_tsc)) {
			pit_purge_expired_with_time(conf->pit, &cur_tsc);
			prev_pit_purge_tsc = cur_tsc;
		}

		/* Read packet from RX queues */
		for (i = 0; i < conf->nb_rx_ports; i++) {
			port_id = conf->rx_queue[i].port_id;
			queue_id = conf->rx_queue[i].queue_id;

			nb_rx = rte_eth_rx_burst((uint8_t) port_id, queue_id, pkts_burst, MAX_PKT_BURST);
			
			// Prefetch each received packet and call forward function
			// since packets are forwarded in burst.			
			if (nb_rx == 0) {
				continue;
			}
			RTE_LOG(DEBUG, AUGUSTUS, "LCORE_%u: Received burst of %u packets "
					"from (port=%u, queue=%u)\n",
					lcore_id, nb_rx, port_id, queue_id);

			/* Prefetch the first PREFETCH_OFFSET packets */
			for (j = 0; j < PREFETCH_OFFSET && j < nb_rx; j++) {
				rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[j], void *));
				rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[j] + RTE_CACHE_LINE_SIZE, void *));
				RTE_LOG(DEBUG, AUGUSTUS, "LCORE_%u: Prefetch pkt #%d\n", lcore_id, j);
			}
			/*
			 * Prefetch each time 1 packet and forward 1 already prefetched
			 * packet until there are no more packets to prefetch.
			 *
			 * This paced prefetching helps ensuring no cache thrashing happens.
			 * In fact L1 cache has limited size (32KB on all most recent x86
			 * architectures, e.g. Nehalem, Sandy Bridge, Haswell). Prefetching
			 * more packets would certainly lead to thrashing.
			 *
			 * Maybe thrashing could be further reduced by prefetching fewer
			 * packets in L1 and prefetch all other packets in L3, then do
			 * 1-by-1 prefetching, but the availability of the packet in L3
			 * before prefetching requires a smaller prefetch window.
			 */
			for (j = 0; j < (nb_rx - PREFETCH_OFFSET); j++) {
				rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[j + PREFETCH_OFFSET], void *));
				rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[j + PREFETCH_OFFSET] + RTE_CACHE_LINE_SIZE, void *));
				RTE_LOG(DEBUG, AUGUSTUS, "LCORE_%u: Prefetch pkt #%d\n", lcore_id, j + PREFETCH_OFFSET);
				RTE_LOG(DEBUG, AUGUSTUS, "LCORE_%u: Handle pkt #%d\n", lcore_id, j);
				icn_fwd(pkts_burst[j], port_id, conf, tx_mbufs);
			}
			/*
			 * After all packets have been prefetched, forward remaining
			 * (already prefetched) packets
			 */
			for (; j < nb_rx; j++) {
				RTE_LOG(DEBUG, AUGUSTUS, "LCORE_%u: Handle pkt #%d\n", lcore_id, j);
				icn_fwd(pkts_burst[j], port_id, conf, tx_mbufs);
			}
		}
	}
	return 0;
}
