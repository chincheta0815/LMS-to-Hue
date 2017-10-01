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


#ifndef __HUE_LIGHTS_H
#define __HUE_LIGHTS_H


#include "hue_common.h"
#include "hue_configuration.h"
#include "hue_interface.h"


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


extern int hue_get_all_lights(hue_bridge_t *hue_bridge);
extern int hue_get_new_lights(hue_bridge_t *hue_bridge);
extern int hue_search_for_new_lights(hue_bridge_t *hue_bridge);
extern int hue_get_light_attributes_and_state(hue_bridge_t *hue_bridge, hue_light_t *hue_light);
extern int hue_light_attributes_rename(hue_bridge_t *hue_bridge, hue_light_t *hue_light);
extern int hue_set_light_state(hue_bridge_t *bridge, hue_light_t *light, int arg_count, ... );
extern int hue_delete_light(hue_bridge_t *hue_bridge);

#endif
