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

#include <stdbool.h>
#include "squeezeitf.h"
#include "mdnssd-itf.h"
#include "hue_bridge.h"
#include "hue_rest.h"
#include "util.h"

#define MAX_RENDERERS 32
#define SCAN_TIMEOUT  15
#define SCAN_INTERVAL 30

enum {CONFIG_CREATE, CONFIG_UPDATE, CONFIG_MIGRATE};

typedef struct sHuebridgeReq {
    char Type[20];
    union {
        float Volume;
        u64_t FlushTS;
    } Data;
} tHuebridgeReq;

typedef struct sMRConfig {
    bool Enabled;
    int  RemoveTimeout;
    bool AutoPlay;
    char UserName[_STR_LEN_];
    char ClientKey[_STR_LEN_];
} tMRConfig;

struct sMR {
    bool                   On;
    bool                   Running;
    int                    SqueezeHandle;
    sq_dev_param_t         sq_config;
    sq_action_t            sqState;
    char                   UDN[_STR_LEN_];
    char                   FriendlyName[_STR_LEN_];
    pthread_t              Thread;
    pthread_mutex_t        Mutex;
    pthread_cond_t         Cond;
    tMRConfig              Config;
    u32_t                  Expired;
    struct huebridgecl_s   *HueBridge;
    struct in_addr         IPAddress;
    char                   ModelId[_STR_LEN_];
    char                   BridgeId[_STR_LEN_];
    tQueue                 Queue;
    bool                   TrackRunning;
    u32_t                  TrackDuration;
    u8_t                   MetadataWait;
    u32_t                  MetadataHash;
    u8_t                   Volume;
};

extern char glInterface[];
extern tMRConfig glMRConfig;
extern struct sMR glMRDevices[MAX_RENDERERS];
extern sq_dev_param_t glDeviceParam;
extern u32_t glScanInterval;
extern u32_t glScanTimeout;
extern s32_t glLogLimit;

#endif /* __SQUEEZE2HUE_H */
