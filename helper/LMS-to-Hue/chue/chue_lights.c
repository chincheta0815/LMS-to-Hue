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

#include "chue_lights.h"
#include "chue_log.h"

static chue_loglevel_t *clp = &chue_loglevel;

/*
 * @desc Parses the json response containing the state and attributes of all lights
 *
 * @param json response from a hue bridge of type char
 * @param hue_bridge of type chue_bridge_t
 * @return true for success or false for error
 */
bool chue_get_all_lights_parse_response_body(chue_bridge_t *bridge, char *body) {
    json_t *json;
    json = json_loads(body, NULL, NULL);
    if (!json) {
        CHUE_ERROR("Cloud not create json object for decoding");

        return false;
    }
    if (!json_is_object(json)) {
        CHUE_ERROR("Hue connection error (%s)", chue_common_get_hue_error_description(json));
        json_decref(json);

        return false;
    }

    int i = 0;
    void *iter = json_object_iter(json);
    while (iter) {
        chue_light_t light;
            
        json_t *value;
        light.attribute.id = atoi(json_object_iter_key(iter));
        value = json_object_iter_value(iter);
        if ( json_is_object(value) ) {
            json_t *state;
            state = json_object_get(value, "state");

            light.state.on = json_boolean_value(json_object_get(state, "on"));
            light.state.bri = json_integer_value(json_object_get(state, "bri"));
            light.state.hue = json_integer_value(json_object_get(state, "hue"));
            light.state.sat = json_integer_value(json_object_get(state, "sat"));
            light.state.xy[0] = json_real_value(json_array_get(json_object_get(state, "xy"), 0));
            light.state.xy[1] = json_real_value(json_array_get(json_object_get(state, "xy"), 1));
            light.state.ct = json_integer_value(json_object_get(state, "ct"));
            strcpy(light.state.alert, json_string_value(json_object_get(state, "alert")));
            strcpy(light.state.effect, json_string_value(json_object_get(state, "effect")));
            strcpy(light.state.colormode, json_string_value(json_object_get(state, "colormode")));
            light.state.reachable = json_boolean_value(json_object_get(state, "reachable"));
        }
        strcpy(light.attribute.type, json_string_value(json_object_get(value, "type")));
        strcpy(light.attribute.name, json_string_value(json_object_get(value, "name")));
        strcpy(light.attribute.modelid, json_string_value(json_object_get(value, "modelid")));
        strcpy(light.attribute.swversion, json_string_value(json_object_get(value, "swversion")));

        bridge->lights[i] = light;
        bridge->num_lights = i+1;
        i++;
        
        iter = json_object_iter_next(json, iter);
    }
    json_decref(json);

    return true;
}

/*
 * @desc Dumps all light states to a file containing a respective body
 *
 * @param hue bridge of type chue_bridge_t
 * @param filename of type char*
 * @return true for success or false for error
 */
extern bool chue_dump_all_light_states_to_file(chue_bridge_t *bridge) {
    json_t *json = json_object();
    char file[CHUE_STR_LEN+1];
    strcpy(file, chue_common_bridge_get_cached_state_file(bridge));
    
    for ( int i = 0; i < bridge->num_lights; i++) {
        json_t *state;
        state = chue_set_light_state_create_json_body(bridge, bridge->lights[i],
                            8,
                            "BRI", chue_common_int2string(bridge->lights[i].state.bri),
                            "HUE", chue_common_int2string(bridge->lights[i].state.bri),
                            "SAT", chue_common_int2string(bridge->lights[i].state.bri),
                            "CT", chue_common_int2string(bridge->lights[i].state.bri),
                            "XY", chue_common_xy2string(bridge->lights[i].state.xy),
                            "ALERT", bridge->lights[i].state.alert,
                            "EFFECT", bridge->lights[i].state.effect,
                            "TRANSITIONTIME", chue_common_int2string(bridge->lights[i].state.transitiontime)
        );
        json_object_set_new(json, chue_common_int2string(bridge->lights[i].attribute.id), state);
    }
    
    // Now write the states and attributes to the file
    json_dump_file(json, file, JSON_ENCODE_ANY | JSON_INDENT(1));
    json_decref(json);

    return true;
}

/*
 * @desc Restores all light states from a file containing a respective body
 *
 * @param hue_bridge of type chue_bridge_t
 * @param filename of type char*
 * @return true for success or false for error
 */
extern bool chue_restore_all_light_states_from_file(chue_bridge_t *bridge) {
    char file[CHUE_STR_LEN+1];
    strcpy(file, chue_common_bridge_get_cached_state_file(bridge));

    // read in the states and attributes from file
    FILE *fp;
    fp = fopen(file, "r");
    
    char *filecontent = 0;
    int clength;
    if (fp) {
        fseek(fp, 0, SEEK_END);
        clength = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        filecontent = malloc(clength+1);
        if (filecontent) fread(filecontent, 1, clength, fp);
        fclose(fp);
        filecontent[clength] = '\0';
    }
    
    // parse the json body in the file
    chue_get_all_lights_parse_response_body(bridge, filecontent);

    // send the states to all lights of the bridge
    for (int i = 0; i < bridge->num_lights; i++) {
        CHUE_SDEBUG("Restoring state of light with id %d", bridge->lights[i].attribute.id);
        chue_set_light_state(bridge, bridge->lights[i],
                            8,
                            "BRI", chue_common_int2string(bridge->lights[i].state.bri),
                            "HUE", chue_common_int2string(bridge->lights[i].state.bri),
                            "SAT", chue_common_int2string(bridge->lights[i].state.bri),
                            "CT", chue_common_int2string(bridge->lights[i].state.bri),
                            "XY", chue_common_xy2string(bridge->lights[i].state.xy),
                            "ALERT", bridge->lights[i].state.alert,
                            "EFFECT", bridge->lights[i].state.effect,
                            "TRANSITIONTIME", chue_common_int2string(bridge->lights[i].state.transitiontime)
        );
    }

    return true;
}

/*
 * @desc Gets state and attributes of all lights
 *
 * @param hue_bridge of type chue_bridge_t
 * @return true for success or false for error
 */
extern bool chue_get_all_lights(chue_bridge_t *bridge) {
    chue_request_t request;
    chue_response_t response;
    json_t *json = json_object();

    if (chue_connect(bridge)) {
        CHUE_ERROR("Could not connect to hue device");
        return false;
    }
    
    snprintf(request.uri, CHUE_STR_LEN,
         "/api/%s/lights",
         bridge->user_name
    );
    
    request.method = _GET;
    
    request.body = json_dumps(json, JSON_ENCODE_ANY | JSON_INDENT(1));
    json_decref(json);

    chue_send_request(bridge, &request);
    free(request.body);

    chue_receive_response(bridge, &response);
    if (response.body) {

        chue_get_all_lights_parse_response_body(bridge, response.body);

        free(response.body);
    }

    chue_disconnect(bridge);
    
    return true;
}

extern bool chue_get_new_lights(chue_bridge_t *bridge) {
    CHUE_ERROR("Function hue_get_new_lights not yet implemented");
    return false;
}

extern bool chue_search_for_new_lights(chue_bridge_t *bridge){
    CHUE_ERROR("Function hue_search_for_new_lights not yet implemented");
    return false;
}

extern bool chue_get_light_attributes_and_state(chue_bridge_t *bridge, chue_light_t light) {
    CHUE_ERROR("Function hue_light_attributes_and_state not yet implemented");
    return false;
}

extern bool chue_light_attributes_rename(chue_bridge_t *bridge, chue_light_t light) {
    CHUE_ERROR("Function hue_light_attributes_rename not yet implemented");
    return false;
}

/*
 * @desc Create the json body for setting the state of a light
 *
 * @param hue_bridge of type chue_bridge_t
 * @param hue_light of type chue_light_t
 * @param arg_count number of arguments passed to function
 * @param ... arguments of type char*
 * @return true for success or false for error
 */
extern json_t *chue_set_light_state_create_json_body(chue_bridge_t *bridge, chue_light_t light, int arg_count, ...) {
    json_t *json;
    va_list args;
    va_start(args, arg_count);
    json = chue_set_light_state_create_json_body_va(bridge, light, arg_count, args);
    va_end(args);
    
    return json;
}

/*
 * @desc Create the json body for setting the state of a light (VIRIADIC version)
 *
 * @param hue_bridge of type chue_bridge_t
 * @param hue_light of type chue_light_t
 * @param arg_count number of arguments passed to function
 * @param args of type va_list
 * @return true for success or false for error
 */
extern json_t *chue_set_light_state_create_json_body_va(chue_bridge_t *bridge, chue_light_t light, int arg_count, va_list args) {
    json_t *json = json_object();

    char *command;
    char *command_arg;
    
    int i = 0;
    while ( i < (arg_count*2) ) {
        command = va_arg(args, char*);
        i++;
        command_arg = va_arg(args, char*);
        i++;

        if ( strcmp(command, "SWITCH") == 0 ) {
            if( strcmp(command_arg, "ON") == 0 ){
                light.state.on = true;
                json_object_set_new(json, "on", json_true());
                CHUE_SDEBUG("Setting SWITCH to %s", command_arg);
            }
            else if ( strcmp(command_arg, "OFF") == 0 ) {
                light.state.on = false;
                json_object_set_new(json, "on", json_false());
                CHUE_SDEBUG("Setting SWITCH to %s", command_arg);
            }
            else {
                chue_disconnect(bridge);
                CHUE_ERROR("Command SWITCH=%s unknown ", command_arg);
                return false;
            }
        }
        else if ( strcmp(command, "BRI") == 0 ) {
            light.state.bri = atoi(command_arg);
            json_object_set_new(json, "bri", json_integer(light.state.bri));
            CHUE_SDEBUG("Setting BRI to %d", light.state.bri);
        }
        else if ( strcmp(command, "HUE") == 0 ) {
            light.state.hue = atoi(command_arg);
            json_object_set_new(json, "hue", json_integer(light.state.hue));
            CHUE_SDEBUG("Setting HUE to %d", light.state.hue);
        }
        else if ( strcmp(command, "SAT") == 0 ) {
            light.state.sat = atoi(command_arg);
            json_object_set_new(json, "sat", json_integer(light.state.sat));
            CHUE_SDEBUG("Setting SAT to %d", light.state.sat);
        }
        else if ( strcmp(command, "XY") == 0 ) {
            light.state.xy[0] = atof(strtok(command_arg, ","));
            light.state.xy[1] = atof(strtok(NULL, ","));
            CHUE_SDEBUG("Setting XY to [%f, %f]", light.state.xy[0], light.state.xy[1]);
        }
        else if ( strcmp(command, "CT") == 0 ) {
            light.state.ct = atoi(command_arg);
            json_object_set_new(json, "ct", json_integer(light.state.ct));
            CHUE_SDEBUG("Setting CT to %d", light.state.ct);
        }
        else if ( strcmp(command, "ALERT") == 0 ) {
            strcpy(light.state.alert, command_arg);
            json_object_set_new(json, "alert", json_string(light.state.alert));
            CHUE_SDEBUG("Setting ALERT to %s", light.state.alert);
        }
        else if ( strcmp(command, "EFFECT") == 0 ) {
            strcpy(light.state.effect, command_arg);
            json_object_set_new(json, "effect", json_string(light.state.effect));
            CHUE_SDEBUG("Setting EFFECT to %s", light.state.effect);
        }
        else if ( strcmp(command, "TRANSITIONTIME") == 0 ) {
            light.state.transitiontime = atoi(command_arg);
            json_object_set_new(json, "transitiontime", json_integer(light.state.transitiontime));
            CHUE_SDEBUG("Setting TRANSITIONTIME to %d", light.state.transitiontime);
        }
        else if ( strcmp(command, "BRI_INC") == 0 ) {
            CHUE_WARN("Command %s not yet implemented", command);
        }
        else if ( strcmp(command, "SAT_INC") == 0 ) {
            CHUE_WARN("Command %s not yet implemented", command);
        }
        else if ( strcmp(command, "HUE_INC") == 0 ) {
            CHUE_WARN("Command %s not yet implemented", command);
        }
        else if ( strcmp(command, "CT_INC") == 0 ) {
            CHUE_WARN("Command %s not yet implemented", command);
        }
        else if ( strcmp(command, "XY_INC") == 0 ) {
            CHUE_WARN("Command %s not yet implemented", command);
        }
        else {
            CHUE_ERROR("Issued unknown hue state request");
        }
    }
    
    return json;
}

/*
 * @desc Sets the state of a light
 *
 * @param hue_bridge of type chue_bridge_t
 * @param hue_light of type chue_light_t
 * @return true for success or false for error
 */
extern bool chue_set_light_state(chue_bridge_t *bridge, chue_light_t light, int arg_count, ... ) {
    chue_request_t request;
    chue_response_t response;
    json_t *json = json_object();

    if (chue_connect(bridge)) {
        CHUE_ERROR("Could not connect to hue device");
        return false;
    }

    snprintf(request.uri, CHUE_STR_LEN,
             "/api/%s/lights/%d/state",
             bridge->user_name, light.attribute.id
    );

    request.method = _PUT;

    va_list args;
    va_start(args, arg_count);
    json = chue_set_light_state_create_json_body_va(bridge, light, arg_count, args);
    va_end(args);
    
    request.body = json_dumps(json, JSON_ENCODE_ANY | JSON_INDENT(1));
    json_decref(json);

    chue_send_request(bridge, &request);
    free(request.body);

    chue_receive_response(bridge, &response);
    if (response.body) free(response.body);

    chue_disconnect(bridge);
    
    return true;
}

extern bool chue_delete_light(chue_bridge_t *bridge) {
    CHUE_ERROR("Function hue_delete_light not yet implemented");
    
	return false;
}
