/*
 *  Squeeze2Raop - LMS to Raop gateway
 *
 *  Squeezelite : (c) Adrian Smith 2012-2014, triode1@btinternet.com
 *  Additions & gateway : (c) Philippe 2016, philippe_44@outlook.com
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

#ifndef __SQUEEZE2HUE_H
#define __SQUEEZE2HUE_H

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>

#include "upnp.h"
#include "pthread.h"
#include "squeezedefs.h"
#include "squeezeitf.h"
#include "util.h"

#include "libhuec.h"

/*----------------------------------------------------------------------------*/
/* typedefs */
/*----------------------------------------------------------------------------*/

#define MAX_PROTO               128
#define MAX_RENDERERS           32
#define RESOURCE_LENGTH         250
#define	SCAN_TIMEOUT            15
#define SCAN_INTERVAL           30

typedef struct sHueReq {
                char Type[20];
                union {
                    u8_t Volume;
                } Data;
} tHueReq;

typedef struct sHBConfig {
                bool Enabled;
				char Name[SQ_STR_LENGTH];
				char UserName[SQ_STR_LENGTH];
                int	RemoveCount;
} tHBConfig;


struct sHB {
    bool InUse;
    tHBConfig Config;
    sq_dev_param_t sq_config;
    bool on;
    hue_bridge_t    Hue;
    char Manufacturer[RESOURCE_LENGTH];
    char UDN[RESOURCE_LENGTH];
    char FriendlyName[RESOURCE_LENGTH];
    char PresURL[RESOURCE_LENGTH];
    char DescDocURL[RESOURCE_LENGTH];
    bool			TimeOut, Connected;
    int	 			SqueezeHandle;
    sq_action_t		sqState;
    u8_t			Volume;
    int				MissingCount;
    bool			Running;
    pthread_t		Thread;
    pthread_mutex_t Mutex;
    pthread_cond_t	Cond;
    tQueue			Queue;
    u32_t			TrackDuration;
    bool			TrackRunning;
    u8_t			MetadataWait;
    u32_t			MetadataHash;
};

extern char glInterface[];
extern s32_t glLogLimit;
extern tHBConfig glHBConfig;
extern sq_dev_param_t glDeviceParam;
extern char glSQServer[SQ_STR_LENGTH];
extern u32_t glScanInterval;
extern u32_t glScanTimeout;
extern struct sHB glHBDevices[MAX_RENDERERS];


#endif
