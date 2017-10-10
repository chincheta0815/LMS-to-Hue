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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jansson.h"

#include "chue_common.h"
#include "chue_log.h"

static chue_loglevel_t *clp = &chue_loglevel;

/*----------------------------------------------------------------------------*/
extern chue_bridge_t *chue_contruct_bridge(){
    CHUE_SDEBUG("Constructing bridge structure");
    chue_bridge_t *bridge = malloc(sizeof(chue_bridge_t));
    strcpy(bridge->cache_dir, "/tmp");
    
    return bridge;
}

/*----------------------------------------------------------------------------*/
extern bool chue_destroy_bridge(chue_bridge_t *bridge){
    CHUE_SDEBUG("Destroying bridge structure");
    free(bridge);

    return true;
}

/*----------------------------------------------------------------------------*/
extern char *chue_common_get_hue_error_description(json_t *json){
    char *chue_error_string;
    chue_error_string = "no error";
    json_t *data, *key, *value;

    if (json_is_array(json)){
        data = json_array_get(json, 0);
        key = json_object_get(data, "error");
        value = json_object_get(key, "description");
        asprintf(&chue_error_string,
                 "%s",
                 json_string_value(value)
        );
    }
    json_decref(json);
    
    return chue_error_string;
}

/*----------------------------------------------------------------------------*/
extern char *chue_common_bridge_get_cached_state_file(chue_bridge_t *bridge){
    char *cached_state_file;
    CHUE_SDEBUG("Getting cache file from bridge %s", bridge->name);
    asprintf(&cached_state_file,
         "%s/%s",
         bridge->cache_dir, bridge->cache_state_file
    );

    return cached_state_file;
}

/*----------------------------------------------------------------------------*/
extern char *chue_common_int2string(int integer_value) {
    char *string;
    CHUE_SDEBUG("Converting integer value %d to string", integer_value);
    asprintf(&string, "%d", integer_value);
    
    return string;
}

/*----------------------------------------------------------------------------*/
extern char *chue_common_float2string(float float_value) {
    char *string;
    CHUE_SDEBUG("Converting float value %f to string", float_value);
    asprintf(&string, "%f", float_value);
    
    return string;
}

/*----------------------------------------------------------------------------*/
extern char *chue_common_xy2string(float xy_array[2]) {
    char *string;
    CHUE_SDEBUG("Converting xy array [%f,%f] to string", xy_array[0],xy_array[1]);
    asprintf(&string, "%f,%f", xy_array[0], xy_array[1]);
    
    return string;
}
