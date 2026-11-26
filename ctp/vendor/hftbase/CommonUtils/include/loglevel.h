/*
 * loglevel.h
 *
 *  Created on: 28-Oct-2013
 *      Author: sunder
 */

#ifndef LOGLEVEL_H_
#define LOGLEVEL_H_

#define LOG_LEVEL_1

#ifdef LOG_LEVEL_5
#define LOG_LEVEL_4 LOG_LEVEL_5
#endif

#ifdef LOG_LEVEL_4
#define LOG_LEVEL_3 LOG_LEVEL_4
#endif

#ifdef LOG_LEVEL_3
#define LOG_LEVEL_2 LOG_LEVEL_3
#endif

#ifdef LOG_LEVEL_2
#define LOG_LEVEL_1 LOG_LEVEL_2
#endif

#ifdef LOG_LEVEL_5
#define L5(A) A
#else
#define L5(A) /*A*/
#endif

#ifdef LOG_LEVEL_4
#define L4(A) A
#else
#define L4(A) /*A*/
#endif

#ifdef LOG_LEVEL_3
#define L3(A) A
#else
#define L3(A) /*A*/
#endif

#ifdef LOG_LEVEL_2
#define L2(A) A
#else
#define L2(A) /*A*/
#endif

#ifdef LOG_LEVEL_1
#define L1(A) A
#else
#define L1(A) /*A*/
#endif

#include "logger.h"
#endif /* LOGLEVEL_H_ */
