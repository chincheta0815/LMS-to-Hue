/*
 *  Squeeze2hue - LMS to Hue gateway
 *
 *  (c) Philippe, philippe_44@outlook.com
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

#ifndef __CONF_UTIL_H
#define __CONF_UTIL_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ixml.h"
#include "squeeze2hue.h"

extern void MakeMacUnique(struct sMR *Device);
extern void SaveConfig(char *name, void *ref, int mode);
extern void *LoadConfig(char *name, tMRConfig *Conf, sq_dev_param_t *sq_conf);
extern void *FindMRConfig(void *ref, char *UDN);
extern void *LoadMRConfig(void *ref, char *UDN, tMRConfig *Conf, sq_dev_param_t *sq_conf);

#endif
