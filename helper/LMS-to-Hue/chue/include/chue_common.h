/*
 *  chue - philips hue library for C
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


#ifndef __CHUE_COMMON_H
#define __CHUE_COMMON_H

#define CHUE_VERSION "1.0"
#define CHUE_MAX_LIGHTS_PER_BRIDGE 50
#define CHUE_STR_LEN	128

#include <stdbool.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "jansson.h"

typedef int sockfd;


typedef struct chue_capabilities_s {
	int avail_lights;
} chue_capabilities_t;

typedef struct chue_light_state_s {
	bool on;
	int bri;
	int hue;
	int sat;
	float xy[2];
	int ct;
	char alert[CHUE_STR_LEN+1];
	char effect[CHUE_STR_LEN+1];
	int transitiontime;
	//bri_inc;
	//sat_inc;
	//hue_inc;
	//ct_inc;
	//xy_inc;
	char colormode[CHUE_STR_LEN+1];
	bool reachable;
} chue_light_state_t;

typedef struct chue_light_attribute_s {
    bool active;
	int id;
	char type[CHUE_STR_LEN+1];
    char name[CHUE_STR_LEN+1];
	char modelid[CHUE_STR_LEN+1];
	char swversion[CHUE_STR_LEN+1];
} chue_light_attribute_t;

typedef struct chue_light_s {
	chue_light_state_t state;
	chue_light_attribute_t attribute;
} chue_light_t;


typedef struct chue_bridge_s {
	sockfd sock;
	char api_version[CHUE_STR_LEN+1];
	struct in_addr ip_address;
	uint8_t mac[6];
	char name[CHUE_STR_LEN+1];
	char user_name[CHUE_STR_LEN+1];
	chue_capabilities_t capabilities;
	int num_lights;
	chue_light_t lights[CHUE_MAX_LIGHTS_PER_BRIDGE];
    char cache_dir[CHUE_STR_LEN+1];
    char cache_state_file[CHUE_STR_LEN+1];
} chue_bridge_t;

typedef struct chue_request_s {
	int method;
	char uri[CHUE_STR_LEN + 1];
	char *body;
} chue_request_t;

typedef struct chue_response_s {
    int status_code;
    char *body;
} chue_response_t;

extern chue_bridge_t *chue_construct_bridge();
extern bool chue_destroy_bridge(chue_bridge_t *bridge);
extern char *chue_common_get_hue_error_description(json_t *json);
extern char *chue_common_bridge_get_cached_state_file(chue_bridge_t *bridge);
extern char *chue_common_float2string(float float_value);
extern char *chue_common_int2string(int integer_value);
extern char *chue_common_xy2string(float xy_array[2]);

#endif
