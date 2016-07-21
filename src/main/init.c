/*
 * Lorenzo Saino, Massimo Gallo
 *
 * Copyright (c) 2016 Alcatel-Lucent, Bell Labs
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <rte_lcore.h>
#include <rte_ethdev.h>
#include <rte_ether.h>

#include <config.h>
#include <util.h>

#include <fib/fib.h>
#include <pit/pit.h>
#include <cs/cs.h>

#define MYPORT 9000    // the port users will be connecting to

#include "data_plane.h"
#include "init.h"

#define INIT_LOG(...) printf("[INIT]: " __VA_ARGS__)

#define LINK_STATUS_CHECK_INTERVAL 100 /* 100ms */

#define LINK_STATUS_MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */

/* Check the link status of all ports in up to 9s, and print them finally */
static int check_all_ports_link_status(uint32_t portmask) {

	uint8_t port_id, count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;

	INIT_LOG("Checking link status...\n");
	for (count = 0; count <= LINK_STATUS_MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		for (port_id = 0; port_id < 4*sizeof(portmask); port_id++) {
			if ((portmask & (1 << port_id)) == 0) {
				/* Skip disabled ports */
				continue;
			}
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(port_id, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status) {
					INIT_LOG("Port %d: Link up, speed: %u Mbps, %s\n",
							 (uint8_t) port_id, (unsigned)link.link_speed,
							 (link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
							 ("full-duplex") : ("half-duplex\n"));
				} else {
					INIT_LOG("Port %d: Link down\n", (uint8_t)port_id);
				}
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == 0) {
				all_ports_up = 0;
				break;
			}
		}
		/* after finally printing all link status, get out */
		if (print_flag == 1) {
			break;
		}

		/* If at least one port is not yet up, wait and retry */
		if (all_ports_up == 0) {
			rte_delay_ms(LINK_STATUS_CHECK_INTERVAL);
		}

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (LINK_STATUS_MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			INIT_LOG("All links are up\n");
		}
	}
	return (all_ports_up == 1) ? 0 : -1;
}


/**
 * Init an mbuf pool of nb_mbuf packets each of size mbuf_size on each NUMA
 * socket on which there is at least one lcore.
 *
 * If number of cores are not equally distributed among NUMA sockets, then
 * the lcores running on the most crowded NUMA core will have a smaller average
 * amount of memory per core.
 *
 */
static int
init_mbuf_pools(struct app_global_config *app, struct app_lcore_config lcore[])
{
	int socket_id;
	unsigned lcore_id;
	char pool_name[64];
	struct rte_mempool *pool[APP_MAX_SOCKETS];
	/* This loop is needed */
	for (socket_id = 0; socket_id < APP_MAX_SOCKETS; socket_id++) {
		pool[socket_id] = NULL;
	}
	for (lcore_id = 0; lcore_id < APP_MAX_LCORES; lcore_id++) {
		lcore[lcore_id].pktmbuf_pool = NULL;

		if (!rte_lcore_is_enabled(lcore_id)) {
			continue;
		}
		socket_id = rte_lcore_to_socket_id(lcore_id);
		if (socket_id >= APP_MAX_SOCKETS) {
			rte_exit(EXIT_FAILURE, "Socket %d of lcore %u is out of range %d\n",
				socket_id, lcore_id, APP_MAX_SOCKETS);
		}

		if (pool[socket_id] == NULL) {
			snprintf(pool_name, sizeof(pool_name), "mbuf_pool_%d", socket_id);
			pool[socket_id] =
				rte_mempool_create(
						pool_name,					// Name
						app->nb_mbuf, 				// Number of elements
						app->mbuf_size, 			// Size of each element
						app->mempool_cache_size,	// Per-lcore cache size
						sizeof(struct rte_pktmbuf_pool_private),	// private data size
						rte_pktmbuf_pool_init, NULL,	// pointer to func init mempool	and args
						rte_pktmbuf_init, NULL,			// pointer to func init mbuf and args
						socket_id,				// socket ID
						0);						// flags

			if (pool[socket_id] == NULL) {
				rte_exit(EXIT_FAILURE,
						"Cannot init mbuf pool on socket %d\n", socket_id);
			} else {
				INIT_LOG("Allocated mbuf pool on socket %d\n", socket_id);
			}
		}
		lcore[lcore_id].pktmbuf_pool = pool[socket_id];
	}
	return 0;
}

static int start_ports(uint32_t portmask, uint8_t promisc_mode) {
	int ret;
	uint8_t port_id, nb_ports;

	/* Get number of ports enabled via command line */
	nb_ports = rte_eth_dev_count();

	/* Start all ports after all configuration has been done */
	for (port_id = 0; port_id < nb_ports; port_id++) {
		if ((portmask & (1 << port_id)) == 0) {
			continue;
		}
		ret = rte_eth_dev_start(port_id);
		if (ret < 0) {
			rte_exit(EXIT_FAILURE, "rte_eth_dev_start: err=%d, port=%u\n",
				  ret, port_id);
		}
		if (promisc_mode == 1) {
			rte_eth_promiscuous_enable(port_id);
		} else {
			rte_eth_promiscuous_disable(port_id);
		}

		INIT_LOG("Successfully set up port %u\n", port_id);
	}
	return check_all_ports_link_status(portmask);
}


/**
 * Init all RX and TX queues
 *
 * @params portmask
 *   User-provided port mask
 */
static void init_queues(uint32_t portmask, struct app_lcore_config lcore[]) {
	// Iterate over cores and ports to enable HW queues and map them to cores

	int ret;
	/*
	 * queue_id is needed to keep track of the queue IDs assigned to each
	 * lcore because there can be holes in the sequence of lcore IDs
	 * (e.g. 1, 2, 4) but there cannot be holes in the RX and TX queues
	 * lists. 
	 */
	uint8_t port_id, queue_id;
	uint8_t socket_id, lcore_id;
	uint8_t nb_ports;
	uint8_t nb_ports_available;

	/* Get number of ports enabled via command line */
	nb_ports = rte_eth_dev_count();
	nb_ports_available = get_nb_ports_available(portmask);
	for (lcore_id = 0, queue_id = 0; lcore_id < APP_MAX_LCORES; lcore_id++) {
		/* if lcore is not enabled, skip queues initialization */
		if ((!rte_lcore_is_enabled(lcore_id))||(lcore_id == CONTROL_PLANE_LCORE)) {
			continue;
		}
		lcore[lcore_id].nb_rx_ports = nb_ports_available;
		lcore[lcore_id].nb_ports = nb_ports;
		socket_id = (uint8_t) rte_lcore_to_socket_id(lcore_id);

		for (port_id = 0; port_id < nb_ports; port_id++) {
			/* Skip queue initialization for disabled ports */
			if ((portmask & (1 << port_id)) == 0) {
				continue;
			}

			lcore[lcore_id].rx_queue[port_id].port_id = port_id;
			lcore[lcore_id].rx_queue[port_id].queue_id = queue_id;

			//TODO: Review rx_conf, nv_rxd
			ret = rte_eth_rx_queue_setup(port_id, (uint16_t) queue_id, nb_rxd,
					socket_id,	&rx_conf, lcore[lcore_id].pktmbuf_pool);
			if (ret < 0) {
				rte_exit(EXIT_FAILURE,
						"rte_eth_rx_queue_setup:err=%d, port=%u\n",
					  ret, (unsigned) port_id);
			}
			/* init one TX queue on each port
			 *
			 * TODO: If all lcores write on the same queue_id for each port,
			 * then a scalar value is fine. This structure allows more
			 * flexibility. See whether I should improve the situation at some
			 * point
			 */
			lcore[lcore_id].tx_queue_id[port_id] = queue_id;
			ret = rte_eth_tx_queue_setup(port_id, queue_id, nb_txd, socket_id, &tx_conf);
			if (ret < 0)
				rte_exit(EXIT_FAILURE,
						"rte_eth_tx_queue_setup:err=%d, port=%u\n",
						ret, (unsigned) port_id);
		}
		queue_id++;
	}
}


/**
 * @param portmask
 *   The mask of enabled ports passed via command line
 * @param nb_rx_queues
 *   The number of RX queues enabled in each port
 * @param nb_tx_queues
 *   The number of TX queues enabled in each port
 *
 */
static int
init_ports(uint32_t portmask, uint8_t nb_rx_queues, uint8_t nb_tx_queues) {
	int ret;
	uint8_t port_id, nb_ports, nb_ports_available;
	/*
	 * This data structure is used to retrieve properties from NICs, such max
	 * nb of rx and tx queues, offload capabilities and Jumbo frames support.
	 */
	struct rte_eth_dev_info dev_info;

	/*
	 * Get the number of Ethernet devices initialized. Depends on the EAL
	 * arguments specified when launching the program (Ethernet device
	 * white/black-listing) and devices actually available in the machine and
	 * attached to the DPDK driver.
	 *
	 * From now on, all devices whose port identifier is in the range
	 * [0,  rte_eth_dev_count() - 1] can be operated on by network applications
	 */
	nb_ports = rte_eth_dev_count();
	if (nb_ports == 0) {
		rte_exit(EXIT_FAILURE, "No Ethernet ports available. "
				 "Did you attach the NICs to the DPDK driver?\n");
	} else if (nb_ports == 1) {
		INIT_LOG("Only one Ethernet port available to DPDK on this machine\n");
	} else if (nb_ports > APP_MAX_ETH_PORTS) {
		nb_ports = APP_MAX_ETH_PORTS;
	}
	INIT_LOG("Recognized %u ports on this machine.\n", nb_ports);

	nb_ports_available = nb_ports;

	/*
	 * Initialize each port
	 */
	for (port_id = 0; port_id < nb_ports; port_id++) {
		/*
		 * Skip ports that are not enabled, i.e. that are not included in the
		 * portmask provided by the user at launch time
		 */
		if ((portmask & (1 << port_id)) == 0) {
			INIT_LOG("Skipping disabled port %u\n", port_id);
			nb_ports_available--;
			continue;
		}

		/*
		 * I must have a dedicated hardware RX and TX queue per logical core to
		 * operate correctly. These queues cannot be shared by cores without
		 * using locks, which detriment performance
		 */
		rte_eth_dev_info_get(port_id, &dev_info);
		if (nb_rx_queues > dev_info.max_rx_queues) {
			rte_exit(EXIT_FAILURE, "NIC %s has only %u hardware RX queues",
					 dev_info.driver_name, dev_info.max_rx_queues);
		}
		if (nb_tx_queues > dev_info.max_tx_queues) {
			rte_exit(EXIT_FAILURE, "NIC %s has only %u hardware TX queues",
					 dev_info.driver_name, dev_info.max_tx_queues);
		}
		/*
		 * Make sure that the NIC supports Jumbo frames
		 *
		 */
		if (dev_info.max_rx_pktlen < 8192) {
			rte_exit(EXIT_FAILURE, "NIC %s does not support Jumbo frames",
					 dev_info.driver_name);
		}
		/*
		 * Configure a single port by specifying how many TX and RX
		 * hardware queues have to be used in a device. Each RX and TX queue
		 * can be identified with an integer from 0 to N-1.
		 *
		 * Note that in this specific case I enable as many RX and TX queues
		 * as the number of lcores.
		 *
		 * This is not sufficient to init the RX and TX queues. I later need
		 * to set up rx/tx queues with calls to rte_eth_rx_queue_setup
		 * and rte_eth_tx_queue_setup
		 *
		 * The port_conf variable contains low-level configuration for the
		 * NIC such as RSS configuration, checksum calculation offload,
		 * VLAN and JumboFrame support. It is defined further up in this file
		 */
		ret = rte_eth_dev_configure(port_id, nb_rx_queues, nb_tx_queues, &port_conf);
		if (ret < 0) {
			rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
					ret, port_id);
		}
		INIT_LOG("Initialized port %u with %u rx queues and %u tx queues\n",
				port_id, nb_rx_queues, nb_tx_queues);
	}
	if (!nb_ports_available) {
		rte_exit(EXIT_FAILURE, "All available ports are disabled. "
				"Please set portmask.\n");
	}
	return 0;
}


static void init_addr_table(uint32_t portmask, struct app_lcore_config lcore[], char config_remote_addr[][18]) {
	unsigned socket_id, lcore_id;
	uint8_t port_id, nb_ports, port;
	uint8_t ref_lcore_per_socket[APP_MAX_SOCKETS];
	struct port_addr port_addr[APP_MAX_ETH_PORTS];

	nb_ports = rte_eth_dev_count();

	/*
	 * Find out what cores are on what NUMA sockets, this is needed to
	 * distribute, later in this function, one copy of each MAC address table
	 * to a NUMA socket
	 */
	for (socket_id = 0; socket_id < APP_MAX_SOCKETS; socket_id++) {
		for (lcore_id = 0; lcore_id < APP_MAX_LCORES; lcore_id++) {
			if ((!rte_lcore_is_enabled(lcore_id))||(lcore_id == CONTROL_PLANE_LCORE)) {
				continue;
			}
			if (rte_lcore_to_socket_id(lcore_id) == socket_id) {
				ref_lcore_per_socket[socket_id] = lcore_id;
				break;
			}
		}
	}
	port = 0;
	for (port_id = 0; port_id < nb_ports; port_id++) {
		/*
		 * Skip ports that are not enabled, i.e. that are not included in the
		 * portmask provided by the user at launch time
		 */
		if ((portmask & (1 << port_id)) == 0) {
			parse_ether_addr("00:00:00:00:00:00", &port_addr[port_id].remote_addr);
			parse_ether_addr("00:00:00:00:00:00", &port_addr[port_id].local_addr);
			continue;
		}
		/*
		 * Get all MAC addresses of all ports used (will be needed later on
		 * for packet forwarding)
		 */
		rte_eth_macaddr_get(port_id, &port_addr[port_id].local_addr);
		parse_ether_addr(config_remote_addr[port++], &port_addr[port_id].remote_addr);
		
		if (is_same_ether_addr(&port_addr[port_id].local_addr, &port_addr[port].remote_addr)) {
			rte_exit(EXIT_FAILURE, "Local and remote MAC addresses on port %u are identical\n",
							port_id);
		}
	}
	/*
	 * Make a copy of each address table for each NUMA socket, for performance
	 * reasons and assign them to lcores
	 */
	for (lcore_id = 0; lcore_id < APP_MAX_LCORES; lcore_id++) {
		if ((!rte_lcore_is_enabled(lcore_id) )||(lcore_id == CONTROL_PLANE_LCORE)) {
			continue;
		}
		socket_id = rte_lcore_to_socket_id(lcore_id);

		if (socket_id >= APP_MAX_SOCKETS) {
			rte_exit(EXIT_FAILURE, "Socket %u of lcore %u is out of range %u\n",
				socket_id, lcore_id, APP_MAX_SOCKETS);
		}

		for (port_id = 0; port_id < nb_ports; port_id++) {
			ether_addr_copy(&port_addr[port_id].local_addr,
					&lcore[lcore_id].port_addr[port_id].local_addr);
		}
	}
}

static void init_fwd_data_structures(struct app_global_config *app,
	struct app_lcore_config lcore[]) {
	uint8_t lcore_id, socket_id;
	fib_t *fibs[APP_MAX_SOCKETS];
	fib_t *fib;

	/* Reset fibs array */
	for (socket_id = 0; socket_id < APP_MAX_SOCKETS; socket_id++) {
		fibs[socket_id] = NULL;
	}

	for (lcore_id = 0; lcore_id < APP_MAX_LCORES; lcore_id++) {
		/* if lcore is not enabled, skip its initialization */
		if ((!rte_lcore_is_enabled(lcore_id))||(lcore_id == CONTROL_PLANE_LCORE)) {
			continue;
		}
		socket_id = rte_lcore_to_socket_id(lcore_id);
		if (fibs[socket_id] == NULL) {
			fib = fib_create(app->fib_num_buckets,
					app->fib_max_elements, app->fib_bf_size, socket_id);
		}
		lcore[lcore_id].fib = fib;

		lcore[lcore_id].pit = pit_create(app->pit_num_buckets,
				app->pit_max_elements, socket_id,
				app->pit_ttl_us);

		lcore[lcore_id].cs = cs_create(app->cs_num_buckets,
				app->cs_max_elements, socket_id);
	}
}

void init_fib_update_process(int *sockfd)
{
	struct addrinfo *servinfo, *p;
	struct sockaddr_in hints;
	int numbytes;
	struct sockaddr_storage their_addr;

	char *IPaddr = "127.0.0.1";

	socklen_t addr_len;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.sin_family = AF_INET; // set to AF_INET to force IPv4
	hints.sin_port = htons(MYPORT);

	inet_pton(AF_INET, IPaddr, &hints.sin_addr);

	if (( (*sockfd) = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
		perror("listener: socket");
		return;
	}

	if (bind((*sockfd), (struct sockaddr*) &hints , sizeof(hints) ) == -1)
	{
		perror("listener: bind");
		return;
	}
}

void init_app(struct app_global_config *app, struct app_lcore_config lcore[]) {
	uint8_t nb_lcores = get_nb_lcores_available();
	nb_lcores -= 1; //One core is reserved for control plane
	
	INIT_LOG("Initializing mbuf pools\n");
	init_mbuf_pools(app, lcore);
	INIT_LOG("Initializing ICN forwarding data structures (FIB, PIT, CS)\n");
	init_fwd_data_structures(app, lcore);
	INIT_LOG("Initializing ports\n");
	init_ports(app->portmask, nb_lcores, nb_lcores);
	INIT_LOG("Initializing hardware queues\n");
	init_queues(app->portmask, lcore);
	INIT_LOG("Starting ports\n");
	start_ports(app->portmask, app->promic_mode);
	INIT_LOG("Setting MAC address table\n");
	init_addr_table(app->portmask, lcore, app->config_remote_addr);
	init_fib_update_process(&(app->sockfd));
	INIT_LOG("Initializing FIB table update process\n");
	INIT_LOG("Initialization complete\n");
}





