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

#include <string.h>

#include "jansson.h"

#include "chue_configuration.h"
#include "chue_log.h"

static chue_loglevel_t *clp = &chue_loglevel;

uint8_t *chue_stringMac2uint8_t(const char *stringMac) {
    int i;
    static uint8_t uint8tmac[6];
    
    for (i = 0; i < 6; i++) {
        uint8tmac[i] = strtol(stringMac + i*3, NULL, 16);
    }
    
    return uint8tmac;

}

/**
 * @desc Gets the configuration of the hue bridge
 *
 * @param pointer to a variable of type chue_bridge_t
 * @return 0 for success or 1 for error
 */
extern bool chue_get_bridge_config(chue_bridge_t *bridge) {

	json_t *root, *api_version, *mac_addr, *name;
	json_error_t error;
	chue_request_t request;
	chue_response_t response;

	if (chue_connect(bridge)) {
		return false;
	}

	request.method = _GET;

	snprintf(request.uri, CHUE_STR_LEN,
		 "/api/%s/config",
		 bridge->user_name
	);
    
	// send empty body for getting the configuration
	request.body = "";

	chue_send_request(bridge, &request);
	chue_receive_response(bridge, &response);

	chue_disconnect(bridge);

	root = json_loads(response.body, 0, &error);
	if (response.body) free(response.body);

	if (!root) {
		CHUE_ERROR("Error root.\n");
		return false;
	}

	if (!json_is_object(root)) {
		CHUE_ERROR("Error object.\n");
		json_decref(root);
		return false;
	}

	api_version = json_object_get(root, "apiversion");
	// will be zero-terminated as the whole context if NULL'd at init
	strncpy(bridge->api_version, json_string_value(api_version), CHUE_STR_LEN);

	// maybe make a function for that conversion.
	mac_addr = json_object_get(root, "mac");
	memcpy(bridge->mac, chue_stringMac2uint8_t(json_string_value(mac_addr)), sizeof(bridge->mac));
	snprintf(bridge->cache_state_file, CHUE_STR_LEN,
             "huebridge_%02x%02x%02x%02x%02x%02x.state",
             bridge->mac[0], bridge->mac[1], bridge->mac[2], bridge->mac[3], bridge->mac[4], bridge->mac[5]);

	name = json_object_get(root, "name");
	// will be zero-terminated as the whole context if NULL'd at init
	strncpy(bridge->name, json_string_value(name), CHUE_STR_LEN);

	json_decref(root);

	return true;
}
