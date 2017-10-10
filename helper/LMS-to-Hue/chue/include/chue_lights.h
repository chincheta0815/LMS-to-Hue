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


#ifndef __CHUE_LIGHTS_H
#define __CHUE_LIGHTS_H

#include "jansson.h"

#include "chue_common.h"
#include "chue_configuration.h"
#include "chue_interface.h"


enum {  SWITCH,
        BRI,
        HUE,
        SAT,
        XY,
        CT,
        ALERT,
        EFFECT,
        TRANSITIONTIME,
        BRI_INC,
        SAT_INC,
        HUE_INC,
        CT_INC,
        XY_INC
};


extern bool chue_get_all_lights(chue_bridge_t *bridge);
extern bool chue_get_new_lights(chue_bridge_t *bridge);
extern bool chue_search_for_new_lights(chue_bridge_t *bridge);
extern bool chue_get_light_attributes_and_state(chue_bridge_t *bridge, chue_light_t light);
extern bool chue_light_attributes_rename(chue_bridge_t *bridge, chue_light_t light);
extern bool chue_dump_all_light_states_to_file(chue_bridge_t *bridge);
extern bool chue_restore_all_light_states_from_file(chue_bridge_t *bridge);
extern json_t *chue_set_light_state_create_json_body_va(chue_bridge_t *bridge, chue_light_t light, int arg_count, va_list args);
extern json_t *chue_set_light_state_create_json_body(chue_bridge_t *bridge, chue_light_t light, int arg_count, ...);
extern bool chue_set_light_state(chue_bridge_t *bridge, chue_light_t light, int arg_count, ... );
extern bool chue_delete_light(chue_bridge_t *bridge);

#endif
