/*
 * Lorenzo Saino, Massimo Gallo
 *
 * Copyright (c) 2016 Alcatel-Lucent, Bell Labs
 *
 */

#ifndef _DATA_PLANE_H_
#define _DATA_PLANE_H_

/**
 * @file
 *
 * Data plane implementation
 */

#include <rte_memory.h>
#include <rte_mempool.h>

#include <config.h>
#include "packet.h"
#include "defaults.h"

#include "init.h"

extern struct app_lcore_config lcore_conf[APP_MAX_LCORES];
extern struct app_global_config app_conf;

/**
 * Reset the statistics
 */
void reset_stats();

/**
 * Print all statistics on screen
 */
void print_stats();

int pkt_fwd_loop(__attribute__((unused)) void *arg);

#endif /* _DATA_PLANE_H_ */
