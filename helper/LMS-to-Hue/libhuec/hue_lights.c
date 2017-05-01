/*
 *  huelic - philips hue library for C
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

#define _GNU_SOURCE

/*
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
*/

#include "jansson.h"

#include "hue_lights.h"


int hue_get_all_lights(hue_bridge_t *hue_bridge) {
    return -1;
}

int hue_get_new_lights(hue_bridge_t *hue_bridge) {
    return -1;
}

int hue_search_for_new_lights(hue_bridge_t *hue_bridge){
    return -1;
}

int hue_get_light_attributes_and_state(hue_bridge_t *hue_bridge, hue_light_t *hue_light) {
    return -1;
}

int hue_light_attributes_rename(hue_bridge_t *hue_bridge, hue_light_t *hue_light) {
    return -1;
}

/**
 * @desc Sets the state of a light
 *
 * @param hue_bridge structure of type hue_bridge_s
 * @param hue_light structure of type hue_light_s
 * @return 0 for success or 1 for error
 */
int hue_set_light_state(hue_bridge_t *bridge, hue_light_t *light, int command, char *command_arg) {
    
	hue_request_t request;
	hue_response_t response;
	json_t *json = NULL;

	if (hue_connect(bridge)) {
		return 1;
	}

	snprintf(request.uri, HB_STR_LEN,
			 "/api/%s/lights/%d/state",
			 bridge->userName, light->attribute.id
	);

	request.method = _PUT;

	switch(command) {
		case(SWITCH):
			if( strcmp(command_arg, "ON") == 0 ){
				json = json_pack("{sb}", "on", true);
			}
			else if( strcmp(command_arg, "OFF") == 0 ){
				json = json_pack("{sb}", "on", false);
			}
			else{
				return 1;
			}
			break;

		case(BRI):
			light->state.bri = atoi(command_arg);
			json = json_pack("{si}", "bri", light->state.bri);
			break;

		case(HUE):
			json = json_pack("{si}", "hue", atoi(command_arg));
			break;

		case(SAT):
			//json = json_pack("{si}", "sat", atoi(command_arg));
			break;

		case(XY):
			break;

		case(CT):
			//json = json_pack("{si}", "ct", atoi(command_arg));
			break;

		case(ALERT):
			break;

		case(EFFECT):
			break;

		case(TRANSITIONTIME):
			light->state.transitiontime = atoi(command_arg);
			json = json_pack("{si}", "transitiontime", light->state.transitiontime);
			break;

		case(BRI_INC):
			break;

		case(SAT_INC):
			break;

		case(HUE_INC):
			break;

		case(CT_INC):
			break;

		case(XY_INC):
			break;

	}

	request.body = json_dumps(json, JSON_ENCODE_ANY | JSON_INDENT(1));

	hue_send_request(bridge, &request);
	hue_receive_response(bridge, &response);

	free(request.body);

	hue_disconnect(bridge);

	return 0;
}

int hue_delete_light(hue_bridge_t *hue_bridge) {
	return -1;
}
