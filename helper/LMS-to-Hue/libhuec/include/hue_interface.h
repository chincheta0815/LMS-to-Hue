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


#ifndef __HUE_INTERFACE_H
#define __HUE_INTERFACE_H


#include "hue_common.h"
#include "hue_configuration.h"


enum { GET, DELETE, POST, PUT };


char *hue_request_method2string(int method_value);

extern int hue_connect(hue_bridge_t *bridge);
extern int hue_disconnect(hue_bridge_t *bridge);
extern int hue_receive_response(hue_bridge_t *bridge, hue_response_t *response);
extern int hue_send_request(hue_bridge_t *bridge, hue_request_t *request);


#endif
