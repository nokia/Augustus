/*
 * Massimo Gallo, Yassine Es-Saiydy
 *
 * Copyright (c) 2016 Alcatel-Lucent, Bell Labs
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <getopt.h>
#include <arpa/inet.h>
#include "../lib/packet.h"

#define SERVERPORT "9000" // the port users sending commands will be connecting to

/* display usage */
static void
print_usage(const char *prgname)
{
	printf ("Usage:\n"
			"  %s -a IPADDRESS -c FIBCOMMAND\n",
			prgname);
}


int main(int argc, char *argv[])
{
        int opt,sockfd;
        struct addrinfo hints, *p;
        int err;
        int numbytes;
	char * address;
	char *command;
	struct icn_hdr hdr;
	struct icn_packet pkt;
	char msg[1500];
	int msg_len = 0;
	uint16_t command_len = 0;
	const char* dest_port = "9000";

	if (argc != 5) {
                fprintf(stderr,"usage: fib_ctrl -a 'address' -c \"command\"\n");
		fprintf(stderr,"     command is of the form (ADD,CLR,DEL):prefix_name:port_id\n");
                exit(1);
        }
	
	while ((opt = getopt(argc, argv, "a:c:")) != EOF) {

		switch (opt) {
		/* address */
		case 'a':
			address = optarg;
			break;

		/* command */
		case 'c':
			command = optarg;
			command_len = strlen(command);
			break;

		default:
			print_usage("fib_ctrl");
			return -1;
		}
	}
	
	printf("Preparing message... \n");

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;

        if ((err = getaddrinfo(address, SERVERPORT, &hints, &p)) != 0) {
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
                return 1;
        }
        


        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                        perror("fib_ctrl: ");
                        return 1;
        }


        pkt.hdr = &hdr;
	pkt.hdr->type = TYPE_CONTROL;

	pkt.hdr->hop_limit = 10;
	pkt.hdr->hdr_len = htons(sizeof(struct icn_hdr));
	pkt.hdr->pkt_len = 0; 			//initialized later on
	pkt.hdr->flags = 0;

	pkt.name = (uint8_t *)command;
	pkt.name_len = htons(command_len);
	
	//NOTE name offset are not used here

	printf("Copying into msg... \n");

	
	//Copy header to message to send
	memcpy(msg, pkt.hdr, ntohs(pkt.hdr->hdr_len));
	msg_len += ntohs(pkt.hdr->hdr_len);
	printf("Copying into msg... \n");
	
	//Copy name length to message to send
	memcpy(&(msg[msg_len]), &(pkt.name_len), 2);
	msg_len += 2;
	
	//Copy name to message to send
	memcpy(&(msg[msg_len]), command, command_len);
	msg_len += command_len;
	
	
	

	pkt.hdr->pkt_len = htons(msg_len);
        if ((numbytes = sendto(sockfd, msg, msg_len, 0, p->ai_addr, p->ai_addrlen)) == -1)
        {
                perror("talker: sendto");
                exit(1);
        }
        
        printf("Sent message %d Bytes, packet len %d, command len %d, command %.*s \n",numbytes, msg_len, ntohs(pkt.name_len), command_len, command );

        close(sockfd);

        return 0;
}