/*
 * Lorenzo Saino, Massimo Gallo
 *
 * Copyright (c) 2016 Alcatel-Lucent, Bell Labs
 *
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_

/**
 * @file
 *
 * Configuration overrides
 *
 * This is a header file supposed to override default configuration parameters
 * included in defaults.h
 */

/*
 * Here below is an example on how to override default configurations
 */

#ifdef PARAM_TO_OVERRIDE
	#undef PARAM_TO_OVERRIDE
#endif
#define PARAM_TO_OVERRIDE NEW_VAL

#include "defaults.h"

#endif /* _CONFIG_H_ */
