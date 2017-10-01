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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jansson.h"
#include "log_util.h"

#include "hue_lights.h"

extern log_level    hue_loglevel;
static log_level    *loglevel = &hue_loglevel;

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
int hue_set_light_state(hue_bridge_t *bridge, hue_light_t *light, int arg_count, ... ) {
    char *command;
    char *command_arg;
    
	hue_request_t request;
	hue_response_t response;
	json_t *json = json_object();

	if (hue_connect(bridge)) {
        LOG_ERROR("Could not connect to hue device");
		return 1;
	}

	snprintf(request.uri, HB_STR_LEN,
			 "/api/%s/lights/%d/state",
			 bridge->userName, light->attribute.id
	);

	request.method = _PUT;

    va_list args;
    va_start(args, arg_count);
    
    int i = 0;
    while (i < (arg_count*2) ) {
        command = va_arg(args, char*);
        i++;
        command_arg = va_arg(args, char*);
        i++;

        if ( strcmp(command, "SWITCH") == 0 ) {
            if( strcmp(command_arg, "ON") == 0 ){
                json_object_set_new(json, "on", json_true());
                LOG_SDEBUG("Setting SWITCH to %s", command_arg);
            }
            else if ( strcmp(command_arg, "OFF") == 0 ) {
                json_object_set_new(json, "on", json_false());
                LOG_SDEBUG("Setting SWITCH to %s", command_arg);
            }
            else {
                hue_disconnect(bridge);
                LOG_ERROR("Command SWITCH=%s unknown ", command_arg);
                return 1;
            }
        }
        else if ( strcmp(command, "BRI") == 0 ) {
            light->state.bri = atoi(command_arg);
            json_object_set_new(json, "bri", json_integer(light->state.bri));
            LOG_SDEBUG("Setting BRI to %s", command_arg);
        }
        else if ( strcmp(command, "HUE") == 0 ) {
            light->state.hue = atoi(command_arg);
            json_object_set_new(json, "hue", json_integer(light->state.hue));
            LOG_SDEBUG("Setting HUE to %s", command_arg);
        }
        else if ( strcmp(command, "SAT") == 0 ) {
            light->state.sat = atoi(command_arg);
            json_object_set_new(json, "sat", json_integer(light->state.sat));
            LOG_SDEBUG("Setting SAT to %s", command_arg);
        }
        else if ( strcmp(command, "XY") == 0 ) {
            LOG_WARN("Command %s not yet implemented", command);
        }
        else if ( strcmp(command, "CT") == 0 ) {
            light->state.ct = atoi(command_arg);
            json_object_set_new(json, "ct", json_integer(light->state.ct));
            LOG_SDEBUG("Setting CT to %s", command_arg);
        }
        else if ( strcmp(command, "ALERT") == 0 ) {
            light->state.alert = command_arg;
            json_object_set_new(json, "alert", json_string(light->state.alert));
            LOG_SDEBUG("Setting ALERT to %s", command);
        }
        else if ( strcmp(command, "EFFECT") == 0 ) {
            light->state.effect = command_arg;
            json_object_set_new(json, "effect", json_string(light->state.effect));
            LOG_SDEBUG("Setting EFFECT to %s", command);
        }
        else if ( strcmp(command, "TRANSITIONTIME") == 0 ) {
            light->state.transitiontime = atoi(command_arg);
            json_object_set_new(json, "transitiontime", json_integer(light->state.transitiontime));
            LOG_SDEBUG("Setting TRANSITIONTIME to %s", command_arg);
        }
        else if ( strcmp(command, "BRI_INC") == 0 ) {
            LOG_WARN("Command %s not yet implemented", command);
        }
        else if ( strcmp(command, "SAT_INC") == 0 ) {
            LOG_WARN("Command %s not yet implemented", command);
        }
        else if ( strcmp(command, "HUE_INC") == 0 ) {
            LOG_WARN("Command %s not yet implemented", command);
        }
        else if ( strcmp(command, "CT_INC") == 0 ) {
            LOG_WARN("Command %s not yet implemented", command);
        }
        else if ( strcmp(command, "XY_INC") == 0 ) {
            LOG_WARN("Command %s not yet implemented", command);
        }
        else {
            LOG_ERROR("Unknown hue state command");
        }
    }
    va_end(args);

	request.body = json_dumps(json, JSON_ENCODE_ANY | JSON_INDENT(1));
	json_decref(json);

	hue_send_request(bridge, &request);
	free(request.body);

	hue_receive_response(bridge, &response);
    if (response.body) free(response.body);

	hue_disconnect(bridge);

	return 0;
}

int hue_delete_light(hue_bridge_t *hue_bridge) {
	return -1;
}
