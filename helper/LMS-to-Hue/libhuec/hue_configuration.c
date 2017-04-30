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

#include "hue_configuration.h"


uint8_t *hue_stringMac2uint8_t(const char *stringMac) {
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
 * @param pointer to a variable of type hue_bridge_t
 * @return 0 for success or 1 for error
 */
extern int hue_get_bridge_config(hue_bridge_t *bridge) {

    json_t *root, *apiVersion, *macAddr, *name;
    json_error_t error;

    hue_request_t *request;
    request = malloc(sizeof(hue_request_t));

    hue_response_t *response;
    response = malloc(sizeof(hue_response_t));

#ifdef _LIBHUEC_USE_PTHREADS
    pthread_mutex_lock(&request->mutex);
#endif

    request->method = GET;
   
    asprintf(&request->uri,
             "/api/%s/config",
             bridge->userName
    );

    // send empty body for getting the configuration
    request->body = "";

    hue_connect(bridge);
    hue_send_request(bridge, request);

#ifdef _LIBHUEC_USE_PTHREADS
    pthread_mutex_unlock(&request->mutex);    
#endif
    free(request);

    hue_receive_response(bridge, response);

#ifdef _LIBHUEC_USE_PTHREADS
    pthread_mutex_lock(&response->mutex);
#endif

    root = json_loads(response->body, 0, &error);
    free(response->body);

#ifdef _LIBHUEC_USE_PTHREADS
    pthread_mutex_unlock(&response->mutex);
#endif
    free(response);
    
    if (!root) {
        printf("Error root.\n");
        return 1;
    }

    if (!json_is_object(root)) {
        printf("Error object.\n");
        json_decref(root);
        return 1;
    }
    
    apiVersion = json_object_get(root, "apiversion");
    strcpy(bridge->apiVersion, json_string_value(apiVersion));

    // maybe make a function for that conversion.
    macAddr = json_object_get(root, "mac");
    memcpy(bridge->mac, hue_stringMac2uint8_t(json_string_value(macAddr)), sizeof(bridge->mac));
    
    name = json_object_get(root, "name");
    strcpy(bridge->name, json_string_value(name)); 

    json_decref(root);

    return 0;
}
