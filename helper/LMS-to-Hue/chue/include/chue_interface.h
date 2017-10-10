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


#ifndef __CHUE_INTERFACE_H
#define __CHUE_INTERFACE_H


#include "chue_common.h"
#include "chue_configuration.h"


enum { _GET, _DELETE, _POST, _PUT };


char *chue_request_method2string(int method_value);

extern int chue_connect(chue_bridge_t *bridge);
extern int chue_disconnect(chue_bridge_t *bridge);
extern int chue_receive_response(chue_bridge_t *bridge, chue_response_t *response);
extern int chue_send_request(chue_bridge_t *bridge, chue_request_t *request);


#endif
