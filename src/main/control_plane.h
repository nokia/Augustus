/*
 * Massimo Gallo, Yassine Es-Saiydy
 *
 * Copyright (c) 2016 Alcatel-Lucent, Bell Labs
 *
 */

#ifndef _CONTROL_PLANE_H_
#define _CONTROL_PLANE_H_

#include <config.h>
#include <sys/socket.h>
#include "init.h"

extern struct app_lcore_config lcore_conf[APP_MAX_LCORES];
extern struct app_global_config app_conf;

void *get_in_addr(struct sockaddr *sa);
int ctrl_loop(__attribute__((unused)) void *arg);

#endif /* _CONTROL_PLANE_H_ */
