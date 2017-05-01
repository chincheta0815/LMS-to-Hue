/*
 *  libhuec - philips hue library for C
 *
 *  (c) Rouven Weiler 2017
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#ifndef __HUE_COMMON_H
#define __HUE_COMMON_H

#define LIBHUEC_VERSION "1.0"
#define MAX_LIGHTS_PER_BRIDGE 50
#define	HB_STR_LEN	128

#if _LIBHUEC_USE_PTHREADS
#include <pthread.h>
#endif

#include "platform.h"

typedef int sockfd;



typedef struct hue_capabilities_s {
	int lights;
} hue_capabilities_t;


typedef struct hue_light_state_s {
	bool on;
	int bri;
	int hue;
	int sat;
	//xy;
	int ct;
	char *alert;
	char *effect;
	int transitiontime;
	//bri_inc;
	//sat_inc;
	//hue_inc;
	//ct_inc;
	//xy_inc;
} hue_light_state_t;

typedef struct hue_light_attribute_s {
	int id;
	char name[HB_STR_LEN+1];
} hue_light_attribute_t;

typedef struct hue_light_s {
	hue_light_state_t state;
	hue_light_attribute_t attribute;
} hue_light_t;


typedef struct hue_bridge_s {
#if _LIBHUEC_USE_PTHREADS
	pthread_mutex_t mutex;
#endif
	sockfd sock;
	char apiVersion[HB_STR_LEN+1];
	struct in_addr ipAddress;
	uint8_t mac[6];
	char name[HB_STR_LEN+1];
	char userName[HB_STR_LEN+1];
	hue_capabilities_t capabilities;
} hue_bridge_t;

typedef struct hue_request_s {
#ifdef _LIBHUEC_USE_PTHREADS
	pthread_mutex_t mutex;
#endif
	int method;
	char uri[HB_STR_LEN + 1];
	char *body;
} hue_request_t;

typedef struct hue_response_s {
#ifdef _LIBHUEC_USE_PTHREADS
	pthread_mutex_t mutex;
#endif
    int status_code;
    char *body;
} hue_response_t;

#endif
