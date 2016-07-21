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
#include <inttypes.h>
#include <sys/types.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>

#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_debug.h>
#include <rte_ethdev.h>

#include <util.h>
#include <config.h>
#include <packet.h>

#include "init.h"
#include "data_plane.h"
#include "control_plane.h"

#include "main.h"

#define MAIN_LOG(...) printf("[MAIN]: " __VA_ARGS__)

struct user_params {
	uint32_t portmask;
	uint8_t numa_on;
	uint8_t promisc_mode;
	char config_remote_addr[APP_MAX_ETH_PORTS][18];
};

/* These are declared as extern in data plane */
struct app_global_config app_conf;
struct app_lcore_config lcore_conf[APP_MAX_LCORES];


/* display usage */
static void
print_usage(const char *prgname)
{
	printf ("Usage:\n"
			"  %s [EAL options] -- -p PORTMASK -P [--no-numa] -m MAC0 [MAC1 .. MACN]\n"
			"  %s [EAL options] -- (--help | -h)\n"
			"  %s [EAL options] -- (--version | -v)\n"
			"\n"
			"Options:\n"
			"  -p PORTMASK:                 Hexadecimal bitmask of ports to configure\n"
			"  -m MAC0 [MAC1 .. MACN]:      list of MAC addresses associated to port0, port1, ..., portN (separated by a space)\n"
			"  -P                           Enable promiscuous mode\n"
			"  --no-numa                    Disable NUMA awareness\n"
			"  -h --help                    Show this help\n"
			"  -v, --version                Show version\n",
			prgname, prgname, prgname);
}

static void print_version() {
	printf("%s\n", AUGUSTUS_VERSION);
}

void parse_mac_config(char* mac_addrs, char config_remote_addr[][18]){
	char* mac;
	int i = 0;
	uint8_t nb_ports;
	
	nb_ports = rte_eth_dev_count();
	mac = strtok (mac_addrs," ");
 
	while (mac != NULL)
	{
		memcpy(config_remote_addr[i], mac, 18);
		i++;
		if ((i > APP_MAX_ETH_PORTS) || (i > nb_ports))
			return;
		mac = strtok (NULL, " ");
	}
	return;
}

static int parse_args(int argc, char **argv, struct user_params *params) {

	#define CMD_LINE_OPT_NO_NUMA "no-numa"
	#define CMD_LINE_OPT_HELP "help"
	#define CMD_LINE_OPT_VERSION "version"

	int opt, ret;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];
	/* option: char *name, int has_arg, int *flag, int val */
	static struct option lgopts[] = {
		{CMD_LINE_OPT_NO_NUMA, no_argument, 0, 0},
		{CMD_LINE_OPT_HELP, no_argument, 0, 0},
		{CMD_LINE_OPT_VERSION, no_argument, 0, 0},
		{NULL, 0, 0, 0}
	};
	argvopt = argv;

	/* Init default options */
	params->numa_on = 1;

	/*
	 * The 3rd argument is a list of short options.
	 * An option character in this string can be followed by a colon (‘:’) to
	 * indicate that it takes a required argument. If an option character is
	 * followed by two colons (‘::’), its argument is optional
	 */
	while ((opt = getopt_long(argc, argvopt, "p:P::h::v::m:", lgopts, &option_index)) != EOF) {

		switch (opt) {

		/* Help */
		case 'h':
			print_usage(prgname);
			rte_exit(EXIT_SUCCESS, NULL);
			break;

		/* Version */
		case 'v':
			print_version();
			rte_exit(EXIT_SUCCESS, NULL);
			break;

		/* Portmask */
		case 'p':
			params->portmask = parse_mask_32(optarg);
			if (params->portmask == 0) {
				print_usage(prgname);
				rte_exit(EXIT_FAILURE, "Invalid portmask");
			}
			break;
			      
		/* Remote mac addresses Configuration */
		case 'm':
			parse_mac_config(optarg,  params->config_remote_addr);
			if (params->config_remote_addr == 0) {
				print_usage(prgname);
				rte_exit(EXIT_FAILURE, "Error in mac addresses");
			}
			break;

		/* Promiscuous mode */
		case 'P':
			MAIN_LOG("Promiscuous mode enabled\n");
			params->promisc_mode = 1;
			break;

		/* Long options */
		case 0:
			if (!strncmp(lgopts[option_index].name, CMD_LINE_OPT_NO_NUMA, sizeof(CMD_LINE_OPT_NO_NUMA))) {
				MAIN_LOG("NUMA is disabled\n");
				params->numa_on = 0;
			} else if (!strncmp(lgopts[option_index].name, CMD_LINE_OPT_HELP, sizeof(CMD_LINE_OPT_HELP))) {
				print_usage(prgname);
				rte_exit(EXIT_SUCCESS, NULL);
			} else if (!strncmp(lgopts[option_index].name, CMD_LINE_OPT_VERSION, sizeof(CMD_LINE_OPT_VERSION))) {
				print_version(prgname);
				rte_exit(EXIT_SUCCESS, NULL);
			}
			break;

		default:
			print_usage(prgname);
			rte_exit(EXIT_FAILURE, "Invalid options");
		}
	}

	if (optind >= 0)
		argv[optind-1] = prgname;

	ret = optind-1;
	optind = 0; /* reset getopt lib */
	return ret;
}


/* Custom handling of signals to handle stats */
static void
signal_handler(int signum) {
	switch(signum) {
	case SIGUSR1:
		MAIN_LOG("Received SIGUSR1. Printing statistics\n");
		print_stats();
		return;
	case SIGUSR2:
		MAIN_LOG("Received SIGUSR2. Resetting statistics\n");
		reset_stats();
		return;
	default:
		return;
	}
}

int MAIN(int argc, char **argv) {

	int ret;
	int i=0;
	int nb_lcore;
	struct user_params params;

	struct lcore_config lcore[APP_MAX_LCORES];
	uint8_t lcore_id;

	/* Associate signal_hanlder function with appropriate signals */
	signal(SIGUSR1, signal_handler);
	signal(SIGUSR2, signal_handler);

	/* Parse EAL arguments and init DPDK EAL
	 *
	 * After this function call, all lcores are initialized in WAIT state and
	 * ready to receive functions to execute
	 */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	argc -= ret;
	argv += ret;

	nb_lcore = rte_lcore_count();
	if (nb_lcore < 2) 
		rte_exit(EXIT_FAILURE, "Too few locres. At least 2 required (one for packet fwd, one for control plane), %d given\n", nb_lcore);
	/*
	 * This call sets log level in the sense that log messages for a lower
	 * layer that this will not be shown but will still take CPU cycles.
	 * To actually remove logging code from the program, set log level in
	 * DPDK config files located in $RTE_SDK/config.
	 */
	rte_set_log_level(RTE_LOG_DEBUG);

	/*
	 * Parse application-specific arguments, which comes after the EAL ones and
	 * are separated from the latter by a double dash (--)
	 */
	ret = parse_args(argc, argv, &params);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid content router arguments\n");

	// Configure the app config object
	app_conf.fib_num_buckets = FIB_NUM_BUCKETS;
	app_conf.fib_max_elements = FIB_MAX_ELEMENTS;

	app_conf.pit_num_buckets = PIT_NUM_BUCKETS;
	app_conf.pit_max_elements = PIT_MAX_ELEMENTS;
	app_conf.pit_ttl_us = PIT_TTL_US;

	/* CS settings */
	app_conf.cs_num_buckets = CS_NUM_BUCKETS;
	app_conf.cs_max_elements = CS_MAX_ELEMENTS;

	/* Packet burst settings */
	app_conf.tx_burst_size = MAX_PKT_BURST;
	app_conf.rx_burst_size = MAX_PKT_BURST;

	/* Packet pool settings */
	app_conf.nb_mbuf = NB_MBUF;
	app_conf.mbuf_size = MBUF_SIZE;
	app_conf.mempool_cache_size = MEMPOOL_CACHE_SIZE;

	/* Other config */
	app_conf.promic_mode = params.promisc_mode;
	app_conf.portmask = params.portmask;
	app_conf.numa_on = params.numa_on;
	
	for(i=0; i< APP_MAX_ETH_PORTS;i++)
		memcpy(app_conf.config_remote_addr[i],params.config_remote_addr[i] ,18);

	init_app(&app_conf, lcore_conf);
	reset_stats();

	MAIN_LOG("All configuration done. Launching worker lcores\n");

	/* launch per-lcore init on every lcore but lcore 0 and control plane lcore*/
	for(lcore_id = 1; lcore_id < nb_lcore; lcore_id++){
		if (lcore_id == CONTROL_PLANE_LCORE)
			continue;
		ret = rte_eal_remote_launch(pkt_fwd_loop, NULL,lcore_id);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "lcore %u busy\n",lcore_id);
	}
	/* launch control plane core if not MASTER (LCORE=0, current)*/
	if (CONTROL_PLANE_LCORE != 0){
		ret = rte_eal_remote_launch(ctrl_loop, NULL, CONTROL_PLANE_LCORE);
		MAIN_LOG("Fwd and Ctrl loops Launched\n");
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "lcore %u busy\n",CONTROL_PLANE_LCORE);
		pkt_fwd_loop(NULL);
	}
	else
		ctrl_loop(NULL); /* launch control plane core if not MASTER (LCORE=0, current)*/
	
	RTE_LCORE_FOREACH_SLAVE(lcore_id)
	{
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}
	return 0;
}

