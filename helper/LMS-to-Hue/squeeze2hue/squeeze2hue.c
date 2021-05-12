/*
 *
 * squeeze2hue - LMS to Hue Entertainment Bridge
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "squeeze2hue.h"
#include "squeezedefs.h"
#include "squeezeitf.h"
#include "conf_util.h"
#include "log_util.h"
#include "util_common.h"
#include "hue_bridge.h"

#define MODEL_NAME_STRING "Hue Entertainment Bridge"
#define MDNS_DISCOVERY_TIME 20

#define SET_LOGLEVEL(log) if (!strcmp(resp, #log"dbg")) {            \
                              char level[20];                        \
                              i = scanf("%s", level);                \
                              log ## _loglevel = debug2level(level); \
                          }

/*----------------------------------------------------------------------------*/
/* GLOBAL VARIABLES */
/*----------------------------------------------------------------------------*/

static pthread_t            glMainThread;
static pthread_t            glmDNSsearchThread;
static pthread_mutex_t      glMainMutex;
static pthread_cond_t       glMainCond;

static bool                 glMainRunning = true;
static bool                 glGracefulShutdown = true;
static bool                 glDiscovery = false;
static bool                 glInteractive = true;
struct sMR                  glMRDevices[MAX_RENDERERS];

static struct in_addr       glHost;
static char                 *glPidFile;
static bool                 glAutoSaveConfigFile = false;
static void                 *glConfigID = NULL;
static char                 glConfigFileName[_STR_LEN_] = "./hue_entertainment_bridge.xml";
static char                 *glHostName;
static char                 glModelName[_STR_LEN_] = MODEL_NAME_STRING;
char                        glInterface[16] = "?";
static struct mDNShandle_s  *glmDNSsearchHandle;
static bool                 glDeviceRegister = false;
int                         glMigration = 0;

static char                 *glLogFile;
s32_t                       glLogLimit = -1;
u32_t                       glScanInterval = SCAN_INTERVAL;
u32_t                       glScanTimeout = SCAN_TIMEOUT;

log_level                   decode_loglevel = lINFO;
log_level                   huebridge_loglevel = lINFO;
log_level                   huestream_loglevel = lINFO;
log_level                   hueprocess_loglevel = lINFO;
log_level                   main_loglevel = lINFO;
log_level                   output_loglevel = lINFO;
log_level                   slimmain_loglevel = lINFO;
log_level                   slimproto_loglevel = lINFO;
log_level                   stream_loglevel = lINFO;
log_level                   util_loglevel = lINFO;

tMRConfig glMRConfig = {
                                true,                   // enabled
                                true,                   // autoplay
                                0,                      // remove timeout
                                "none",                 // user name
                                "none",                 // client key
                       };

static u8_t LMSVolumeMap[101] = {
                                0, 1, 1, 1, 2, 2, 2, 3,  3,  4,
                                5, 5, 6, 6, 7, 8, 9, 9, 10, 11,
                                12, 13, 14, 15, 16, 16, 17, 18, 19, 20,
                                22, 23, 24, 25, 26, 27, 28, 29, 30, 32,
                                33, 34, 35, 37, 38, 39, 40, 42, 43, 44,
                                46, 47, 48, 50, 51, 53, 54, 56, 57, 59,
                                60, 61, 63, 65, 66, 68, 69, 71, 72, 74,
                                75, 77, 79, 80, 82, 84, 85, 87, 89, 90,
                                92, 94, 96, 97, 99, 101, 103, 104, 106, 108, 110,
                                112, 113, 115, 117, 119, 121, 123, 125, 127, 128
                               };

sq_dev_param_t glDeviceParam = {
                                STREAMBUF_SIZE,
                                OUTPUTBUF_SIZE,
                                "aac,aif,flc,mp3,pcm",
                                "?",
                                "",
                                { 0x00,0x00,0x00,0x00,0x00,0x00 },
                                false,
#if defined(RESAMPLE)
                                96000,
                                true,
                                "",
#else
                                44100,
#endif
				{ "" },
                               } ;

/*----------------------------------------------------------------------------*/
/* local variables */
/*----------------------------------------------------------------------------*/
extern log_level               main_loglevel;
static log_level               *loglevel = &main_loglevel;

static char usage[] =
                    VERSION "\n"
                    "See -t for license terms\n"
                    "Usage: [options]\n"
                    "  -s <server>[:<port>]\tConnect to specified server, otherwise uses autodiscovery to find server\n"
                    "  -a <port>[:<count>]\tset inbound port base and range\n"
                    "  -b <address>]\tNetwork address to bind to\n"
                    "  -x <config file>\tread config from file (default is ./config.xml)\n"
                    "  -i <config file>\tdiscover players, save <config file> and exit\n"
                    "  -r \t\t\tregister new devices and exist\n"
                    "  -m <name1,name2...>\texclude from search devices whose model name contains name1 or name 2 ...\n"
                    "  -I \t\t\tauto save config at every network scan\n"
                    "  -f <logfile>\t\tWrite debug to logfile\n"
                    "  -p <pid file>\t\twrite PID in file\n"
                    "  -d <log>=<level>\tSet logging level, logs: all|slimproto|stream|decode|output|web|main|util|huebridge, level: error|warn|info|debug|sdebug\n"
                    "  -M <modelname>\tSet the squeezelite player model name sent to the server (default: " MODEL_NAME_STRING ")\n"
#if LINUX || FREEBSD || SUNOS
                    "  -z \t\t\tDaemonize\n"
#endif
                    "  -Z \t\t\tNOT interactive\n"
                    "  -k \t\t\tImmediate exit on SIGQUIT and SIGTERM\n"
                    "  -t \t\t\tLicense terms\n"
                    "\n"
                    "Build options:"
#if LINUX
                    " LINUX"
#endif
#if WIN
                    " WIN"
#endif
#if OSX
                    " OSX"
#endif
#if FREEBSD
                    " FREEBSD"
#endif
#if SUNOS
                    " SUNOS"
#endif
#if EVENTFD
                    " EVENTFD"
#endif
#if SELFPIPE
                    " SELFPIPE"
#endif
#if WINEVENT
                    " WINEVENT"
#endif
		    "\n\n";

static char license[] =
                    "This program is free software: you can redistribute it and/or modify\n"
                    "it under the terms of the GNU General Public License as published by\n"
                    "the Free Software Foundation, either version 3 of the License, or\n"
                    "(at your option) any later version.\n\n"
                    "This program is distributed in the hope that it will be useful,\n"
                    "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
                    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
                    "GNU General Public License for more details.\n\n"
                    "You should have received a copy of the GNU General Public License\n"
                    "along with this program.  If not, see <http://www.gnu.org/licenses/>.\n\n";

/*----------------------------------------------------------------------------*/
/* prototypes */
/*----------------------------------------------------------------------------*/
static bool AddHueDevice(struct sMR *Device, mDNSservice_t *s);
static void DelHueDevice(struct sMR *Device);

bool sq_callback(sq_dev_handle_t handle, void *caller, sq_action_t action, void *param) {
    struct sMR *Device = caller;

    if (!Device) {
        LOG_ERROR("no caller ID in callback", NULL);
        return false;
    }

    LOG_SDEBUG("callback for %s (%d)", Device->FriendlyName, action);

    pthread_mutex_lock(&Device->Mutex);

    if (!Device->Running) {
        LOG_WARN("[%p]: huebridge has been removed", Device);
        pthread_mutex_unlock(&Device->Mutex);
        return false;
    }

    if (action == SQ_ONOFF) {
        LOG_SDEBUG("[%p]: callback received SQ_ONOFF", Device);
    
        Device->On = *((bool*) param);

        if (!Device->On && Device->Config.AutoPlay)
            sq_notify(Device->SqueezeHandle, Device, SQ_PLAY, NULL, &Device->On);

        if (!Device->On) {
            tHuebridgeReq *Req = malloc(sizeof(tHuebridgeReq));

            QueueFlush(&Device->Queue);
            strcpy(Req->Type, "OFF");
            QueueInsert(&Device->Queue, Req);
            pthread_cond_signal(&Device->Cond);
        }

        LOG_DEBUG("[%p]: huebridge set on/off %d", caller, Device->On);
    }

    if (!Device->On && action != SQ_SETNAME && action != SQ_SETSERVER) {
        LOG_DEBUG("[%p]: huebridge off or not controlled by LMS", Device);
        pthread_mutex_unlock(&Device->Mutex);
        return false;
    }

    switch (action) {	
        case SQ_CONNECT: {
            LOG_SDEBUG("[%p]: callback received SQ_CONNECT", Device);
            tHuebridgeReq *Req = malloc(sizeof(tHuebridgeReq));

            Device->sqState = SQ_PLAY;

            strcpy(Req->Type, "CONNECT");
            QueueInsert(&Device->Queue, Req);
            pthread_cond_signal(&Device->Cond);
            
            break;
        }
        case SQ_FINISHED: {
            LOG_SDEBUG("[%p]: callback received SQ_FINISHED", Device);

            Device->TrackRunning = false;
            Device->TrackDuration = 0;

            break;
        }
        case SQ_METASEND: {
            LOG_SDEBUG("[%p]: callback received SQ_METASEND", Device);

            Device->MetadataWait = 5;

            break;
        }
        case SQ_PAUSE: {
            LOG_SDEBUG("[%p]: callback received SQ_PAUSE", Device);
            tHuebridgeReq *Req = malloc(sizeof(tHuebridgeReq));

            Device->sqState = SQ_PAUSE;
            Device->TrackRunning = false;

            huebridge_pause(Device->HueBridge);

            strcpy(Req->Type, "PAUSE");
            QueueInsert(&Device->Queue, Req);
            pthread_cond_signal(&Device->Cond);
            break;
        }
        case SQ_SETNAME: {
            LOG_SDEBUG("[%p]: callback received SQ_SETNAME", Device);

            strcpy(Device->sq_config.name, (char*) param);

            break;
        } 
        case SQ_SETSERVER: {
            LOG_SDEBUG("[%p]: callback received SQ_SETSERVER");

            strcpy(Device->sq_config.dynamic.server, inet_ntoa(*(struct in_addr*)param));

            break;
        }
        case SQ_STARTED: {
            LOG_SDEBUG("[%p]: callback received SQ_STARTED", Device);

            Device->TrackRunning = true;
            Device->MetadataWait = 1;
            Device->MetadataHash = 0;

            break;
        }
        case SQ_STOP: {
            LOG_SDEBUG("[%p]: callback received SQ_STOP", Device);
            tHuebridgeReq *Req = malloc(sizeof(tHuebridgeReq));

            Device->sqState = SQ_STOP;
            Device->TrackRunning = false;
            Device->TrackDuration = 0;

            huebridge_stop(Device->HueBridge);

            strcpy(Req->Type, "STOP");
            QueueInsert(&Device->Queue, Req);
            pthread_cond_signal(&Device->Cond);

            break;
        }
        case SQ_UNPAUSE: {
            LOG_SDEBUG("[%p]: callback received SQ_UNPAUSE", Device);
            tHuebridgeReq *Req = malloc(sizeof(tHuebridgeReq));

            Device->sqState = SQ_PLAY;
            Device->TrackRunning = true;

            if (*((unsigned*) param))
                huebridge_start_at(Device->HueBridge, TIME_MS2NTP(*((unsigned*) param)));

            strcpy(Req->Type, "UNPAUSE");
            QueueInsert(&Device->Queue, Req);
            pthread_cond_signal(&Device->Cond);

            break;
        }
        case SQ_VOLUME: {
            LOG_SDEBUG("[%p]: callback received SQ_VOLUME", Device);

            tHuebridgeReq *Req = malloc(sizeof(tHuebridgeReq));
            u32_t volume = *(u16_t*) param;
            int i;

            for (i = 100; volume < LMSVolumeMap[i] && i; i--)
                if (Device->Volume == volume)
                    break;

            Device->Volume = volume;

            Req->Data.Volume = Device->Volume;
            strcpy(Req->Type, "VOLUME");
            QueueInsert(&Device->Queue, Req);
            pthread_cond_signal(&Device->Cond);

            break;
        }
        default: {
            LOG_SDEBUG("[%p]: callback received UNKNOWN command (%d)", Device, action);
            break;
        }
    }

    pthread_mutex_unlock(&Device->Mutex);
    return true;
}

static void *PlayerThread(void *args) {
    struct sMR *Device = (struct sMR*) args;

    Device->Running = true;

    while (Device->Running) {
        // tHuebridgeReq is free at end of this thread.
        tHuebridgeReq *req = GetRequest(&Device->Queue, &Device->Mutex, &Device->Cond, 1000);

        // empty means timeout every sec
        if (!req) {
            u32_t now = gettime_ms();

            LOG_DEBUG("[%p]: tick %u", Device, now);

            // after that, only check what's needed when a track is running
            if (!Device->TrackRunning)
                continue;

            pthread_mutex_lock(&Device->Mutex);
            if (Device->MetadataWait && !--Device->MetadataWait) {
                sq_metadata_t metadata;
                u32_t hash;
                bool valid;

                pthread_mutex_unlock(&Device->Mutex);

                valid = sq_get_metadata(Device->SqueezeHandle, &metadata, false);

                // no valid metadata, nothing to update
                if (!valid) {
                    Device->MetadataWait = 5;
                    sq_free_metadata(&metadata);
                    continue;
                }

                // on live stream, get metadata every 5 seconds.
                if (metadata.remote && !metadata.duration)
                    Device->MetadataWait = 5;

                hash = hash32(metadata.title) ^ hash32(metadata.artwork);

                if (Device->MetadataHash != hash) {
                    Device->TrackDuration = metadata.duration;
                    Device->MetadataHash = hash;

                    LOG_INFO("[%p]: idx %d\n\tartist:%s\n\talbum:%s\n\ttitle:%s\n"
                             "\tgenre:%s\n\tduration:%d.%03d\n\tsize:%d\n\tcover:%s",
                             Device, metadata.index, metadata.artist,
                             metadata.album, metadata.title, metadata.genre,
                             div(metadata.duration, 1000).quot,
                             div(metadata.duration,1000).rem, metadata.file_size,
                             metadata.artwork ? metadata.artwork : "");
                } 

                sq_free_metadata(&metadata);
            }
            else {
                pthread_mutex_unlock(&Device->Mutex);
            }

            continue;
        }

        LOG_INFO("[%p]: request to player %s", Device, req->Type);

        if (!strcasecmp(req->Type, "CONNECT")) {
            LOG_INFO("[%p]:  hue streaming connecting ...", Device);

            if (huebridge_connect(Device->HueBridge)) {
                LOG_INFO("[%p]: hue streaming connected", Device);
            }
            else {
                LOG_ERROR("[%p]: hue streaming failed to connect", Device);
            }
        }

        if (!strcasecmp(req->Type, "OFF")) {
            LOG_INFO("[%p]: hue stream disconnecting ...", Device);
            huebridge_disconnect(Device->HueBridge);
            huebridge_sanitize(Device->HueBridge);
        }

        if (!strcasecmp(req->Type, "STOP")) {
        }

        if (!strcasecmp(req->Type, "PAUSE")) {
        }

        if (!strcasecmp(req->Type, "FLUSH")) {
        }

        if (!strcasecmp(req->Type, "VOLUME")) {
        }

        free(req);
    }

    return NULL;
}

char *GetmDNSAttribute(txt_attr_t *p, int count, char *name){
    int j;

    for (j = 0; j < count; j++)
        if ( !strcasecmp(p[j].name, name))
            return strdup(p[j].value);

    return NULL;
}

struct sMR *SearchUDN(char *UDN) {
    int i;
    
    for (i = 0; i < MAX_RENDERERS; i++) {
        if (glMRDevices[i].Running && !strcmp(glMRDevices[i].UDN, UDN))
            return glMRDevices + i;
    }

    return NULL;
}

bool mDNSsearchCallback(mDNSservice_t *slist, void *cookie, bool *stop) {
    int j;
    struct sMR    *Device;
    mDNSservice_t *s;
    u32_t         now = gettime_ms();

    for (s = slist; s && glMainRunning; s = s->next) {

        // ignore announces made on behalf
        if (!s->name || s->host.s_addr != s->addr.s_addr)
            continue;

        // is that device already there
        if ((Device = SearchUDN(s->name)) != NULL) {
            Device->Expired = 0;
            
            // device disconnected
            if (s->expired) {
                if(!huebridge_is_connected(Device->HueBridge) && !Device->Config.RemoveTimeout) {
                        LOG_INFO("[%p]: removing renderer (%s)", Device, Device->FriendlyName);
                        if (Device->SqueezeHandle)
                            sq_delete_device(Device->SqueezeHandle);
                        DelHueDevice(Device);
                }
                else {
                    LOG_INFO("[%p]: keep missing hue entertainment bridge (%s)", Device, Device->FriendlyName);
                    Device->Expired = now ? now : 1;
                }
            }
            continue;
        }

        // disconnect of an unknown device
        if (!s->port && !s->addr.s_addr) {
            LOG_ERROR("unknown device %s disconnected", s->name);
            continue;
        }

        // should not happen
        if (s->expired) {
            LOG_DEBUG("device %s already expired", s->name);
            continue;
        }

        // device creation so search for a free spot
        for (j = 0; j < MAX_RENDERERS && glMRDevices[j].Running; j++);

        // max number of devices is reached
        if (j == MAX_RENDERERS) {
            LOG_ERROR("max number of hue bridge entertainment devices reached", NULL);
            break;
        }

        Device = glMRDevices + j;

        if(AddHueDevice(Device, s) && !glDiscovery) {
            // create a new slimdevice
            Device->SqueezeHandle = sq_reserve_device(Device, &sq_callback);

            if (!*(Device->sq_config.name))
                strcpy(Device->sq_config.name, Device->FriendlyName);
 
            if (!Device->SqueezeHandle || !sq_run_device(Device->SqueezeHandle, Device->HueBridge, &Device->sq_config)) {
                sq_release_device(Device->SqueezeHandle);
                Device->SqueezeHandle = 0;
                LOG_ERROR("[%p]: cannot create squeezelite instance for device %s", Device, Device->FriendlyName);
                DelHueDevice(Device);
            }

        }
    }

    // walk through list of devices with expired timeout and remove them
    for (j = 0; j < MAX_RENDERERS; j++) {
        Device = glMRDevices + j;
        if (!Device->Running || Device->Config.RemoveTimeout <= 0 || !Device->Expired 
                             || now < Device->Expired + Device->Config.RemoveTimeout*1000)
            continue;

        LOG_INFO("[%p]: removing renderer (%s) on timeout", Device, Device->FriendlyName);
 
        if (Device->SqueezeHandle) 
            sq_delete_device(Device->SqueezeHandle);

        DelHueDevice(Device);
    }    

    if (glAutoSaveConfigFile || glDiscovery || glDeviceRegister) {
        LOG_DEBUG("update configuration %s", glConfigFileName);
        SaveConfig(glConfigFileName, glConfigID, false);
    }

    return false;
}

static void *mDNSsearchThread(void *args) {
    // launch mDNS query
    query_mDNS(glmDNSsearchHandle, "_hue._tcp.local", 120,
               glDiscovery ? MDNS_DISCOVERY_TIME : 0, &mDNSsearchCallback, NULL);

    return NULL;
}

bool AddHueDevice(struct sMR *Device, mDNSservice_t *s) {
    pthread_attr_t pattr;
    u32_t mac_size = 6;
    char *bridgeid;
    char *modelid;
    char *str;

    // get bridgeid at the very beginning as it is needed for getting the config.
    bridgeid = GetmDNSAttribute(s->attr, s->attr_count, "bridgeid");

    // read parameters from default then config file
    memcpy(&Device->Config, &glMRConfig, sizeof(tMRConfig));
    memcpy(&Device->sq_config, &glDeviceParam, sizeof(sq_dev_param_t));
    LoadMRConfig(glConfigID, bridgeid, &Device->Config, &Device->sq_config);

    strcpy(Device->UDN, bridgeid);
    NFREE(bridgeid);

    modelid = GetmDNSAttribute(s->attr, s->attr_count, "modelid");
    if (modelid && strcasestr(modelid, "BSB001")) {
        LOG_DEBUG("[%p]: hue bridge (modelid: %s) does not support protocol", Device, modelid);
        NFREE(modelid);
        return false;
    }
    strcpy(Device->ModelId, GetmDNSAttribute(s->attr, s->attr_count, "modelid"));
    NFREE(modelid);

    if (!Device->Config.Enabled)
        return false;

    Device->IPAddress = s->addr;

    strcpy(Device->FriendlyName, s->hostname);
    str = strcasestr(Device->FriendlyName, ".local");
    if (str != NULL)
        *str = '\0';

    Device->On              = false;
    Device->Running         = true;
    Device->SqueezeHandle   = 0;
    Device->sqState         = SQ_STOP;
    Device->HueBridge       = NULL;
    Device->Expired         = 0;
    Device->TrackRunning    = false;
    Device->Volume          = -1;
    Device->MetadataWait    = 0;
    Device->MetadataHash    = 0;

    strcpy(Device->BridgeId, Device->UDN);
    LOG_DEBUG("[%p]: found hue bridge (modelid: %s, bridgeid: %s, ip: %s)",
              Device, Device->ModelId, Device->BridgeId, inet_ntoa(Device->IPAddress));

    if (!memcmp(Device->sq_config.mac, "\0\0\0\0\0\0", mac_size)) {
        if (SendARP(Device->IPAddress.s_addr, INADDR_ANY, (u32_t*) Device->sq_config.mac, &mac_size)) {
            u32_t hash = hash32(Device->UDN);

            LOG_ERROR("[%p]: cannot get mac address for device %s, creating fake %x", Device, Device->FriendlyName, hash);
            memcpy(Device->sq_config.mac + 2, &hash, 4);
        }
        memset(Device->sq_config.mac, 0xaa, 2);
    }

    MakeMacUnique(Device);

    Device->HueBridge = huebridge_create(Device->IPAddress, Device->Config.UserName, Device->Config.ClientKey, FRAMES_PER_BLOCK);

    if (!Device->HueBridge) {
        LOG_ERROR("[%p]: cannot create hue entertainment bridge device", Device);
        return false;
    }

    if (glDeviceRegister) {
        LOG_INFO("[%p]: registering new hue entertainment bridge (%s)", Device, inet_ntoa(Device->IPAddress));

        if (!huebridge_register(Device->HueBridge)) {
            LOG_ERROR("[%p]: could not register hue entertainment bridge device", Device);
        }
        else {
            LOG_DEBUG("[%p]: setting device username (%s) and clientkey (%s)", Device, Device->HueBridge->hue_rest_ctx.username, Device->HueBridge->hue_rest_ctx.clientkey);
            strcpy(Device->Config.UserName, Device->HueBridge->hue_rest_ctx.username);
            strcpy(Device->Config.ClientKey, Device->HueBridge->hue_rest_ctx.clientkey);
        }
    }
    else {
        if (!huebridge_check_prerequisites(Device->HueBridge)) {
            LOG_INFO("[%p]: squeeze2hue could not find needed prerequisites for use with hue entertainment bridge (%s)", Device, inet_ntoa(Device->IPAddress));
            LOG_INFO("[%p]:     hint: app has to be registered and/or at least one entertainment group containing at least one light needs to be defined", Device);
            return false;
        }
    }

    Device->HueBridge->flash_mode = HUE_COLOR_FREQ;

    pthread_mutex_init(&Device->Mutex, 0);
    QueueInit(&Device->Queue);

    pthread_attr_init(&pattr);
    pthread_attr_setstacksize(&pattr, PTHREAD_STACK_MIN + 64*1024);
    pthread_create(&Device->Thread, NULL, &PlayerThread, Device);
    pthread_attr_destroy(&pattr);

    return true;
}

void FlushHueDevices(void) {
    int i;

    for (i = 0; i < MAX_RENDERERS; i++) {
        struct sMR *p = &glMRDevices[i];
        if (p->Running)
            DelHueDevice(p);
    }
}

void DelHueDevice(struct sMR *Device) {
    pthread_mutex_lock(&Device->Mutex);
    Device->Running = false;
    pthread_cond_signal(&Device->Cond);
    pthread_mutex_unlock(&Device->Mutex);

    pthread_join(Device->Thread, NULL);

    huebridge_destroy(Device->HueBridge);

    LOG_INFO("[%p] hue entertainment bridge stopped", Device);

    //NFREE pointers here.
}

static void *MainThread(void *args) {
    while (glMainRunning) {
        pthread_mutex_lock(&glMainMutex);
        pthread_cond_reltimedwait(&glMainCond, &glMainMutex, 30*1000);
        pthread_mutex_unlock(&glMainMutex);

        if (glLogFile && glLogLimit != -1) {
            s32_t size = ftell(stderr);

            if (size > glLogLimit*1024*1024) {
                u32_t sum, bufSize = 16384;
                u8_t *buf = malloc(bufSize);

                FILE *rlog = fopen(glLogFile, "rb");
                FILE *wlog = fopen(glLogFile, "r+b");
                LOG_DEBUG("resizing log", NULL);
                for (sum = 0, fseek(rlog, size - (glLogLimit*1024*1024) / 2, SEEK_SET);
                        (bufSize = fread(buf, 1, bufSize, rlog)) != 0;
                        sum += bufSize, fwrite(buf, 1, bufSize, wlog));

                sum = fresize(wlog, sum);
                fclose(wlog);
                fclose(rlog);
                NFREE(buf);
                if (!freopen(glLogFile, "a", stderr)) {
                    LOG_ERROR("re-open errror while truncating log", NULL);
                }
            }
        }
    }

    return NULL;
}

static bool Start(void) {
    memset(&glMRDevices, 0, sizeof(glMRDevices));

    pthread_mutex_init(&glMainMutex, 0);
    pthread_cond_init(&glMainCond, 0);
    for (int i = 0; i < MAX_RENDERERS; i++) {
        pthread_mutex_init(&glMRDevices[i].Mutex, 0);
        pthread_cond_init(&glMRDevices[i].Cond, 0);
    }

    glHost.s_addr = get_localhost(&glHostName);
    if (!strstr(glInterface, "?")) {
        glHost.s_addr = inet_addr(glInterface);
    }
    LOG_INFO("binding to %s", inet_ntoa(glHost));

    LOG_INFO("initializing hue entertainment interface ...");
    if (huebridge_rest_init()) {
        LOG_ERROR("cannot initialize huebridge rest interface", NULL);
        return false;
    }

    /* start the mDNS devices discovery thread */
    if (( glmDNSsearchHandle = init_mDNS(false, glHost)) == NULL) {
        LOG_ERROR("annot start mDNS discovery", NULL);
        return false;
    }
    pthread_create(&glmDNSsearchThread, NULL, &mDNSsearchThread, NULL);

    /* start the main thread */
    pthread_create(&glMainThread, NULL, &MainThread, NULL);

    return true;
}


static bool Stop(void) {
    int i;

    glMainRunning = false;

    LOG_DEBUG("terminate mDNS search thread ...", NULL);
    close_mDNS(glmDNSsearchHandle);
    pthread_join(glmDNSsearchThread, NULL);

    LOG_DEBUG("flush hue bridge devices ...", NULL);
    FlushHueDevices();

    LOG_INFO("stopping hue entertainment interface ...");
    huebridge_rest_cleanup();

    LOG_DEBUG("terminate main thread ...", NULL);
    pthread_cond_signal(&glMainCond);
    pthread_join(glMainThread, NULL);
    pthread_mutex_destroy(&glMainMutex);
    pthread_cond_destroy(&glMainCond);
    for (i = 0; i < MAX_RENDERERS; i++) {
        pthread_mutex_destroy(&glMRDevices[i].Mutex);
        pthread_cond_destroy(&glMRDevices[i].Cond);
    }

    free(glHostName);

    if (glConfigID) {
        ixmlDocument_free(glConfigID);
    }

#if WIN
    winsock_close();
#endif

    return true;
}

static void sighandler(int signum) {
    static bool quit = false;

    // give it some time to terminate
    if (quit) {
        LOG_INFO("please wait for clean exit!", NULL);
        return;
    }

    quit = true;

    if (!glGracefulShutdown) {
        LOG_INFO("forced exit", NULL);
        exit(EXIT_SUCCESS);
    }

    sq_end();
    Stop();
    exit(EXIT_SUCCESS);
}


/*---------------------------------------------------------------------------*/
bool ParseArgs(int argc, char **argv) {
    char *optarg = NULL;
    int optind = 1;
    int i = 1;

    char cmdline[256] = "";

    for (i = 0; i < argc && (strlen(argv[i]) + strlen(cmdline) + 2 < sizeof(cmdline)); i++) {
        strcat(cmdline, argv[i]);
        strcat(cmdline, " ");
    }

    while (optind < argc && strlen(argv[optind]) >= 2 && argv[optind][0] == '-') {
        char *opt = argv[optind] + 1;
        if (strstr("stxdfpicbM", opt) && optind < argc - 1) {
            optarg = argv[optind + 1];
            optind += 2;
        } else if (strstr("tzZIkr"
#if defined(RESAMPLE)
                          "uR"
#endif
                   , opt)) {
            optarg = NULL;
            optind += 1;
        }
        else {
            printf("%s", usage);

            return false;
        }

        switch (opt[0]) {
            case 'r':
                glDeviceRegister = true;
                break;
            case 's':
                strcpy(glDeviceParam.server, optarg);
                break;
            case 'M':
                strcpy(glModelName, optarg);
                break;
            case 'b':
                strcpy(glInterface, optarg);
                break;
#if defined(RESAMPLE)
            case 'u':
            case 'R':
                if (optind < argc && argv[optind] && argv[optind][0] != '-') {
                    strcpy(glDeviceParam.resample_options, argv[optind++]);
                    glDeviceParam.resample = true;
                } 
                else {
                    strcpy(glDeviceParam.resample_options, "");
                    glDeviceParam.resample = false;
                }
                break;
#endif
            case 'f':
                glLogFile = optarg;
                break;
            case 'i':
                strcpy(glConfigFileName, optarg);
                glDiscovery = true;
                break;
            case 'x':
                strcpy(glConfigFileName, optarg);
                break;
            case 'I':
                glAutoSaveConfigFile = true;
                break;
            case 'p':
                glPidFile = optarg;
                break;
            case 'Z':
                glInteractive = false;
                break;
            case 'k':
                glGracefulShutdown = false;
                break;
#if LINUX || FREEBSD || SUNOS
            case 'z':
                glDaemonize = true;
                break;
#endif
            case 'd': 
                {
                    char *l = strtok(optarg, "=");
                    char *v = strtok(NULL, "=");
                    log_level new = lWARN;
                    if (l && v) {
                        if (!strcmp(v, "error"))  new = lERROR;
                        if (!strcmp(v, "warn"))   new = lWARN;
                        if (!strcmp(v, "info"))   new = lINFO;
                        if (!strcmp(v, "debug"))  new = lDEBUG;
                        if (!strcmp(v, "sdebug")) new = lSDEBUG;
                        if (!strcmp(l, "all") || !strcmp(l, "huebridge")) huebridge_loglevel = new;
                        if (!strcmp(l, "all") || !strcmp(l, "huebridge")) huestream_loglevel = new;
                        if (!strcmp(l, "all") || !strcmp(l, "main")) main_loglevel = new;
                        if (!strcmp(l, "all") || !strcmp(l, "slimmain")) slimmain_loglevel = new;
                        if (!strcmp(l, "all") || !strcmp(l, "slimproto")) slimproto_loglevel = new;
                        if (!strcmp(l, "all") || !strcmp(l, "util")) util_loglevel = new;
                        if (!strcmp(l, "all") || !strcmp(l, "output")) output_loglevel = new;
                    }
                    else {
                        printf("%s", usage);
                        return false;
                    }
                }
                break;
            case 't':
                printf("%s", license);

                return false;
            default:
                break;
        }
    }

    return true;
}


/*----------------------------------------------------------------------------*/
/*																			  */
/*----------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
    int i;
    char resp[20] = "";

    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
#if defined(SIGPIPE)
    signal(SIGPIPE, SIG_IGN);
#endif
#if defined(SIGQUIT)
    signal(SIGQUIT, sighandler);
#endif
#if defined(SIGHUP)
    signal(SIGHUP, sighandler);
#endif

    // first try to find a config file on the command line
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-x")) {
            strcpy(glConfigFileName, argv[i+1]);
        }
    }

    // load config from xml file
    glConfigID = (void*) LoadConfig(glConfigFileName, &glMRConfig, &glDeviceParam);

    // do some parameter migration
    if (!glMigration || glMigration == 1 || glMigration == 2) {
        glMigration = 3;
        if (!strcasestr(glDeviceParam.codecs, "ogg"))
            strcat(glDeviceParam.codecs, ",ogg");
        SaveConfig(glConfigFileName, glConfigID, CONFIG_MIGRATE);
    }

    // potentially overwrite with some cmdline parameters
    if (!ParseArgs(argc, argv))
        exit(1);

    if (glLogFile) {
        if (!freopen(glLogFile, "a", stderr)) {
            fprintf(stderr, "error opening logfile %s: %s\n", glLogFile, strerror(errno));
        }
    }

    LOG_ERROR("Starting squeeze2hue version: %s\n", VERSION);

    if (!glConfigID) {
        LOG_ERROR("\n\n!!!!!!!!!!!!!!!!!! ERROR LOADING CONFIG FILE !!!!!!!!!!!!!!!!!!!!!\n", NULL);
    }

    if (glDiscovery) {
        Start();
        sleep(MDNS_DISCOVERY_TIME + 1);
        Stop();
        return(0);
    }

    if (glDeviceRegister) {
        Start();
        sleep(MDNS_DISCOVERY_TIME + HUE_APP_REGISTER_TIME + 1);
        glMainRunning = false;
        Stop();
        return(0);
    }

#if LINUX || FREEBSD || SUNOS
    if (glDaemonize) {
        if (daemon(1, glLogFile ? 1 : 0)) {
            fprintf(stderr, "error daemonizing: %s\n", strerror(errno));
        }
    }
#endif

    if (glPidFile) {
        FILE *pid_file;
        pid_file = fopen(glPidFile, "wb");
        if (pid_file) {
            fprintf(pid_file, "%d", (int) getpid());
            fclose(pid_file);
        }
        else {
            LOG_ERROR("Cannot open PID file %s", glPidFile);
        }
    }

    sq_init(glModelName);

    if (!Start()) {
        LOG_ERROR("Cannot start", NULL);
        strcpy(resp, "exit");
    }

    while (strcmp(resp, "exit")) {

#if LINUX || FREEBSD || SUNOS
        if (!glDaemonize && glInteractive)
            i = scanf("%s", resp);
        else
            pause();
#else
        if (glInteractive)
            i = scanf("%s", resp);
        else
#if OSX
            pause();
#else
            Sleep(INFINITE);
#endif
#endif

        SET_LOGLEVEL(decode)
        SET_LOGLEVEL(huebridge)
        SET_LOGLEVEL(hueprocess)
        SET_LOGLEVEL(huestream)
        SET_LOGLEVEL(main)
        SET_LOGLEVEL(output)
        SET_LOGLEVEL(slimmain)
        SET_LOGLEVEL(slimproto)
        SET_LOGLEVEL(stream)
        SET_LOGLEVEL(util)

        if (!strcmp(resp, "save")) {
            char name[128];
            i = scanf("%s", name);
            SaveConfig(name, glConfigID, true);
        }

        if (!strcmp(resp, "dump") || !strcmp(resp, "dumpall")) {
            bool all = !strcmp(resp, "dumpall");

            for (i = 0; i < MAX_RENDERERS; i++) {
                struct sMR *p = &glMRDevices[i];
                bool locked = pthread_mutex_trylock(&p->Mutex);

                if (!locked)
                    pthread_mutex_unlock(&p->Mutex);

                if (!p->Running && !all)
                    continue;

                printf("%20.20s [r:%u] [l:%u] [sq:%u] [%s] [mw:%u] [%p::%p]\n",
                       p->FriendlyName, p->Running, locked, p->sqState,
                       inet_ntoa(p->IPAddress), p->MetadataWait,
                       p, sq_get_ptr(p->SqueezeHandle));
            }
        }

    }

    glMainRunning = false;
    LOG_INFO("stopping squeezelite devices ...", NULL);
    sq_end();
    LOG_INFO("stopping Hue Entertainment Bridge devices ...", NULL);
    Stop();
    LOG_INFO("all done", NULL);

    return true;
}
