/*
 * Lorenzo Saino, Massimo Gallo
 *
 * Copyright (c) 2016 Alcatel-Lucent, Bell Labs
 *
 */

#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>

#include "packet.h"
#include <config.h>

uint8_t parse_packet(uint8_t* pkt, struct icn_packet* icn_pkt){
    
	uint8_t*  ptr;   /*Pointer used to parse the packet*/
	uint16_t* name_len;
	uint16_t* type;
	uint16_t* length;
	
	/*Parse the fixed header part*/
	icn_pkt->hdr = (struct icn_hdr *) pkt;
	ptr = (uint8_t *) RTE_PTR_ADD(icn_pkt->hdr, sizeof(struct icn_hdr));

	/*Parse the name length*/
	name_len = (uint16_t*) ptr;
	icn_pkt->name_len = rte_be_to_cpu_16(*name_len);   
	ptr = (uint8_t *) RTE_PTR_ADD(ptr, sizeof(uint16_t));
	
	/*Parse the name */
	icn_pkt->name = (uint8_t *) RTE_PTR_ADD(icn_pkt->hdr, ICN_HDR_NAME_OFFSET);
	ptr = (uint8_t *) RTE_PTR_ADD(icn_pkt->name, icn_pkt->name_len);
	

	/* If packet */ 
	if ((ptr-pkt) >= rte_be_to_cpu_16(icn_pkt->hdr->pkt_len))
		return 1;
	    
	/*Parse the name offsets */
	type = (uint16_t*) ptr;
	ptr = (uint8_t *) RTE_PTR_ADD(ptr, sizeof(uint16_t));
	
	length = (uint16_t*) ptr;
	ptr = (uint8_t *) RTE_PTR_ADD(ptr, sizeof(uint16_t));
	    
	if(*type == TLV_TYPE_NAME_COMPONENTS_OFFSET_BE){
		icn_pkt->component_offsets = ptr;
		icn_pkt->component_offsets_size = rte_be_to_cpu_16(*length);
		icn_pkt->component_nr = icn_pkt->component_offsets_size/2; //length of component offset is 2B
	}
	else
		return 1;
	  
	
	ptr = (uint8_t *) RTE_PTR_ADD(ptr, rte_be_to_cpu_16(*length));
	
	length = (uint16_t*) ptr;
	ptr = (uint8_t *) RTE_PTR_ADD(ptr, sizeof(uint16_t));
	
	ptr = (uint8_t *) RTE_PTR_ADD(ptr, rte_be_to_cpu_16(*length));
	
	return 0;
}
