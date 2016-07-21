/*
 * Massimo Gallo, Yassine Es-Saiydy
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
#include <packet.h>

#include <fib/fib.h>

#include "control_plane.h"

#ifndef NULL
#define NULL   ((void *) 0)
#endif

#define CONTROL_PLANE_LOG(...) printf("[CONTROL PLANE]: " __VA_ARGS__)

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*) sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*) sa)->sin6_addr);
}

int ctrl_loop(__attribute__((unused)) void *arg) {

	struct sockaddr_storage their_addr;
	int numbytes;
	int sockfd;
	int nb_lcore;
	struct icn_packet packet;

	socklen_t addr_len;
	char s[INET6_ADDRSTRLEN];

	struct app_lcore_config *conf;
	unsigned lcore_id, socket_id;

	/* get ID of the lcore on which it is running */
	lcore_id = rte_lcore_id();
	socket_id = rte_socket_id();
	nb_lcore = rte_lcore_count();

	CONTROL_PLANE_LOG("[LCORE_%u] Started\n", lcore_id);

	/* Get core configuration */
	conf = &lcore_conf[lcore_id];

	sockfd = app_conf.sockfd;

	while (1) {
		uint8_t buffer[1500];
		addr_len = sizeof(their_addr);

		if ((numbytes = recvfrom(sockfd, buffer, 1500, 0, (struct sockaddr *)&their_addr, &addr_len)) == -1)
		{
		    perror("recvfrom");
		    exit(1);
		}
		CONTROL_PLANE_LOG("[LCORE_%u] Received %d Bytes message\n", lcore_id, numbytes);

		parse_packet(buffer, &packet);

		char command[3];
		if (packet.name_len <3){
			CONTROL_PLANE_LOG("[LCORE_%u]: Error, invalid FIB update command. Too short.\n", lcore_id);
			continue;
		}

		memcpy(command, packet.name, 3);

		uint8_t * prefix = packet.name + 4;
		uint8_t* prefix_delim = strchr(packet.name + 4, ':');
		if (prefix_delim == NULL) {
			printf("Error, invalid FIB update command\n");
			continue;
		}
		uint16_t prefix_len = prefix_delim - prefix;

		uint8_t* face_delim = packet.name + packet.name_len;
		if (face_delim ==NULL){
		      CONTROL_PLANE_LOG("[LCORE_%u]: Error, invalid FIB update command. No interface\n", lcore_id);
		      continue;
		}
		uint16_t face_len = packet.name_len - prefix_len - 3 /* Command len */ - 2 /* Separators len */;

		char sface[5];
		memcpy(sface, prefix_delim + 1, face_len);
		sface[face_len] = '\0';
		uint16_t face = atoi(sface);

		struct ether_addr empty;
		parse_ether_addr("00:00:00:00:00:00", &empty);

		if (strcmp(command, "ADD") == 0) {
			// ADD prefix to tables in all cores
			for (lcore_id = 0; lcore_id < APP_MAX_LCORES; lcore_id++) {
				if (lcore_conf[lcore_id].fib == NULL) {
					continue;
				}
				if ((face > lcore_conf[lcore_id].nb_ports) || (is_same_ether_addr(&empty,&(lcore_conf[lcore_id].port_addr[face].local_addr)))) {
					CONTROL_PLANE_LOG("Error, invalid interface\n");
					continue;
				}
				int ret = fib_add(lcore_conf[lcore_id].fib, prefix, prefix_len, face);
				if (ret >= 0)
					CONTROL_PLANE_LOG("[LCORE_%u] FIB ENTRY '%.*s' interface %d ADDED\n", lcore_id, (int)prefix_len, (char *)prefix, face);
				else 
					CONTROL_PLANE_LOG("[LCORE_%u] FIB ENTRY ADD '%.*s' interface %d UNSUCCESFUL \n", lcore_id, (int)prefix_len, (char *)prefix, face);

			  }
		}
		else if (strcmp(command, "DEL") == 0){
			for(lcore_id = 0; lcore_id < APP_MAX_LCORES; lcore_id++){	
				if(lcore_conf[lcore_id].fib == NULL){
					    continue;
				}
				if ((face > lcore_conf[lcore_id].nb_ports) || (is_same_ether_addr(&empty,&(lcore_conf[lcore_id].port_addr[face].local_addr)))) {
					CONTROL_PLANE_LOG("Error, invalid interface\n");
					continue;
				}
				int ret = fib_del(lcore_conf[lcore_id].fib, prefix, prefix_len, face);
				if (ret >= 0)
					CONTROL_PLANE_LOG("[LCORE_%u] FIB ENTRY '%.*s' interface %d DELETED\n", lcore_id, (int)prefix_len, (char *)prefix, face);
				else 
					CONTROL_PLANE_LOG("[LCORE_%u] FIB ENTRY DELETE '%.*s' interface %d UNSUCCESFUL \n", lcore_id, (int)prefix_len, (char *)prefix, face);
			  }
		}
	}
	return 0;
}
