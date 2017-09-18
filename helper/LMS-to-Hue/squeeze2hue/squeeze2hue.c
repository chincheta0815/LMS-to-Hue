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

#include <math.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#if WIN
#include <process.h>
#endif

#if SUNOS
#include <limits.h>
#endif

#include "ixml.h"
#include "upnp.h"
#include "upnpdebug.h"
#include "upnptools.h"
#include "squeezedefs.h"
#include "squeeze2hue.h"
#include "conf_util.h"
#include "util_common.h"
#include "log_util.h"
#include "util.h"

#include "libhuec.h"
#include "virtual.h"

/*----------------------------------------------------------------------------*/
/* globals initialized */
/*----------------------------------------------------------------------------*/
#if LINUX || FREEBSD || SUNOS
bool				glDaemonize = false;
#endif
char 				glInterface[16] = "?";
bool				glInteractive = true;
char				*glLogFile;
s32_t				glLogLimit = -1;
static char			*glPidFile = NULL;
static char			*glSaveConfigFile = NULL;
bool				glAutoSaveConfigFile = false;
bool				glGracefullShutdown = true;

log_level	slimproto_loglevel = lINFO;
log_level	stream_loglevel = lWARN;
log_level	decode_loglevel = lWARN;
log_level	output_loglevel = lINFO;
log_level	main_loglevel = lINFO;
log_level	slimmain_loglevel = lINFO;
log_level	util_loglevel = lINFO;
log_level	virtual_loglevel = lINFO;

tHBConfig			glHBConfig = {
							true,   // enabled
                            false,  // connected (valid username)
							"",     // device name
							"none", // user name
							30,     // remove count
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
					"flc,pcm,aif,aac,mp3",
					"?",
					"",
					{ 0x00,0x00,0x00,0x00,0x00,0x00 },
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
/* globals */
/*----------------------------------------------------------------------------*/
static pthread_t 	glMainThread;
void                *glConfigID = NULL;
char                glConfigName[SQ_STR_LENGTH] = "./config.xml";
static bool         glDiscovery = false;
u32_t               glScanInterval = SCAN_INTERVAL;
u32_t               glScanTimeout = SCAN_TIMEOUT;
struct sHB          glHBDevices[MAX_RENDERERS];
pthread_mutex_t     glHBFoundMutex;
char                glUPnPSocket[128] = "?";
unsigned int        glPort;
char                glIPaddress[128] = "";
UpnpClient_Handle   glControlPointHandle;

/*----------------------------------------------------------------------------*/
/* const */
/*----------------------------------------------------------------------------*/

static const char   SEARCH_TARGET[]     = "upnp:rootdevice";
static const char   cPhilipsHueBridge[] = "Philips hue bridge";
static const char   cLogitech[]         = "Logitech";

/*----------------------------------------------------------------------------*/
/* locals */
/*----------------------------------------------------------------------------*/
extern log_level    main_loglevel;
static log_level    *loglevel = &main_loglevel;

static pthread_t    glUpdateHueThread;
static bool         glMainRunning = true;

static struct sLocList {
	char            *Location;
	struct sLocList *Next;
} *glHBFoundList = NULL;


static char usage[] =
			VERSION "\n"
		   "See -t for license terms\n"
		   "Usage: [options]\n"
		   "  -s <server>[:<port>]\tConnect to specified server, otherwise uses autodiscovery to find server\n"
		   "  -b <address>]\tNetwork address to bind to\n"
		   "  -x <config file>\tread config from file (default is ./config.xml)\n"
		   "  -i <config file>\tdiscover players, save <config file> and exit\n"
		   "  -I \t\t\tauto save config at every network scan\n"
		   "  -f <logfile>\t\tWrite debug to logfile\n"
  		   "  -p <pid file>\t\twrite PID in file\n"
		   "  -d <log>=<level>\tSet logging level, logs: all|slimproto|stream|decode|output|web|main|util|raop, level: error|warn|info|debug|sdebug\n"
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
		   "along with this program.  If not, see <http://www.gnu.org/licenses/>.\n\n"
	;

/*----------------------------------------------------------------------------*/
/* prototypes */
/*----------------------------------------------------------------------------*/
static void *UpdateHueThread(void *args);
static bool AddHueDevice(struct sHB *Device, char * UDN, IXML_Document *DescDoc,	const char *location);
void 		DelHueDevice(struct sHB *Device);

/*----------------------------------------------------------------------------*/
bool sq_callback(sq_dev_handle_t handle, void *caller, sq_action_t action, void *param)
{
	struct sHB *device = caller;
	bool rc = true;

	if (!device)	{
		LOG_ERROR("No caller ID in callback", NULL);
		return false;
	}

	if (action == SQ_ONOFF) {
		device->on = *((bool*) param);

		if (!device->on) {
			tHueReq *Req = malloc(sizeof(tHueReq));

			virtual_disconnect(device->vPlayer);

			pthread_mutex_lock(&device->Mutex);
			QueueFlush(&device->Queue);
			strcpy(Req->Type, "OFF");
			QueueInsert(&device->Queue, Req);
			pthread_cond_signal(&device->Cond);
			pthread_mutex_unlock(&device->Mutex);
		}

		LOG_DEBUG("[%p]: device set on/off %d", caller, device->on);
	}

	if (!device->on && action != SQ_SETNAME && action != SQ_SETSERVER) {
		LOG_DEBUG("[%p]: device off or not controlled by LMS", caller);
		return false;
	}

	LOG_SDEBUG("callback for %s (%d)", device->FriendlyName, action);
	pthread_mutex_lock(&device->Mutex);

	switch (action) {
		case SQ_FINISHED:
			device->TrackRunning = false;
			device->TrackDuration = 0;
			break;
		case SQ_STOP: {
			tHueReq *Req = malloc(sizeof(tHueReq));

			device->TrackRunning = false;
			device->TrackDuration = 0;
			device->sqState = SQ_STOP;
			virtual_stop(device->vPlayer);

			strcpy(Req->Type, "STOP");
			QueueInsert(&device->Queue, Req);
			pthread_cond_signal(&device->Cond);
			break;
		}
		case SQ_PAUSE: {
			tHueReq *Req;

			device->TrackRunning = false;
			device->sqState = SQ_PAUSE;
			virtual_pause(device->vPlayer);

			Req = malloc(sizeof(tHueReq));
			strcpy(Req->Type, "PAUSE");
			QueueInsert(&device->Queue, Req);
			pthread_cond_signal(&device->Cond);
			break;
		}
		case SQ_UNPAUSE: {
			tHueReq *Req = malloc(sizeof(tHueReq));

			device->TrackRunning = true;
			device->sqState = SQ_PLAY;

			if (*((unsigned*) param)) virtual_start_at(device->vPlayer, TIME_MS2NTP(*((unsigned*) param)));

			strcpy(Req->Type, "UNPAUSE");
			QueueInsert(&device->Queue, Req);
			pthread_cond_signal(&device->Cond);
			break;
		}
		case SQ_VOLUME: {
			u32_t Volume = *(u16_t*) param;
			tHueReq *Req = malloc(sizeof(tHueReq));
			int i;

			for (i = 100; Volume < LMSVolumeMap[i] && i; i--);
			if (device->Volume == Volume) break;

			device->Volume = Volume;

			Req->Data.Volume = device->Volume;
			strcpy(Req->Type, "VOLUME");
			QueueInsert(&device->Queue, Req);
			pthread_cond_signal(&device->Cond);
			break;
		}
		case SQ_CONNECT: {
			tHueReq *Req = malloc(sizeof(tHueReq));

			device->sqState = SQ_PLAY;
			virtual_connect(device->vPlayer);

			strcpy(Req->Type, "CONNECT");
			QueueInsert(&device->Queue, Req);
			pthread_cond_signal(&device->Cond);

			break;
		}
		case SQ_METASEND:
			device->MetadataWait = 5;
			break;
		case SQ_STARTED:
			device->TrackRunning = true;
			device->MetadataWait = 1;
			device->MetadataHash = 0;
			break;
		case SQ_SETNAME:
			strcpy(device->sq_config.name, (char*) param);
			break;
		case SQ_SETSERVER:
			strcpy(device->sq_config.dynamic.server, inet_ntoa(*(struct in_addr*) param));
			break;
		default:
			break;
	}

	pthread_mutex_unlock(&device->Mutex);
	return rc;
}


/*----------------------------------------------------------------------------*/
int CallbackEventHandler(Upnp_EventType EventType, void *Event, void *Cookie)
{
	LOG_SDEBUG("event: %i [%s] [%p]", EventType, uPNPEvent2String(EventType), Cookie);

	switch ( EventType ) {
		case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
		break;
		case UPNP_DISCOVERY_SEARCH_RESULT: {
			struct Upnp_Discovery *d_event = (struct Upnp_Discovery *) Event;
			struct sLocList **p, *prev = NULL;

			LOG_SDEBUG("Answer to uPNP search %d", d_event->Location);
			if (d_event->ErrCode != UPNP_E_SUCCESS) {
				LOG_SDEBUG("Error in Discovery Callback -- %d", d_event->ErrCode);
				break;
			}

			ithread_mutex_lock(&glHBFoundMutex);
			p = &glHBFoundList;
			while (*p) {
				prev = *p;
				p = &((*p)->Next);
			}
			(*p) = (struct sLocList*) malloc(sizeof (struct sLocList));
			(*p)->Location = strdup(d_event->Location);
			(*p)->Next = NULL;
			if (prev) prev->Next = *p;
			ithread_mutex_unlock(&glHBFoundMutex);
			break;
		}
		case UPNP_DISCOVERY_SEARCH_TIMEOUT:	{
			pthread_attr_t attr;

			pthread_attr_init(&attr);
			pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN + 32*1024);
			pthread_create(&glUpdateHueThread, &attr, &UpdateHueThread, NULL);
			pthread_detach(glUpdateHueThread);
			pthread_attr_destroy(&attr);
			break;
		}
		case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE: {
			struct Upnp_Discovery *d_event = (struct Upnp_Discovery *) Event;
			struct sHB *p;
			int i;

			for (i = 0; i < MAX_RENDERERS; i++) {
				if (!glHBDevices[i].InUse) continue;
				if (!strcmp(glHBDevices[i].UDN, d_event->DeviceId)) break;
			}

			if (i == MAX_RENDERERS) break;

			p = glHBDevices + i;

			ithread_mutex_lock(&p->Mutex);

			if (!*d_event->ServiceType && p->Connected) {
				p->Connected = false;
				LOG_INFO("[%p]: Player BYE-BYE", p);
			}

			ithread_mutex_unlock(&p->Mutex);

			break;
		}
		case UPNP_EVENT_AUTORENEWAL_FAILED:
		case UPNP_EVENT_SUBSCRIPTION_EXPIRED:
		case UPNP_CONTROL_GET_VAR_COMPLETE:
		case UPNP_EVENT_RENEWAL_COMPLETE:
		case UPNP_EVENT_SUBSCRIBE_COMPLETE:
		case UPNP_EVENT_SUBSCRIPTION_REQUEST:
		case UPNP_CONTROL_ACTION_REQUEST:
		case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
		case UPNP_CONTROL_GET_VAR_REQUEST:
		case UPNP_CONTROL_ACTION_COMPLETE:
		case UPNP_EVENT_RECEIVED:
		break;
	}

	Cookie = Cookie;
	return 0;
}


/*----------------------------------------------------------------------------*/
static void *PlayerThread(void *args)
{
	struct sHB *Device = (struct sHB*) args;

	Device->Running = true;

	while (Device->Running) {
		tHueReq *req = GetRequest(&Device->Queue, &Device->Mutex, &Device->Cond, 1000);

		// empty means timeout every sec
		if (!req) {
			u32_t now = gettime_ms();

			LOG_DEBUG("[%p]: tick %u", Device, now);

			// after that, only check what's needed when running
			if (!Device->TrackRunning) continue;

			pthread_mutex_lock(&Device->Mutex);
			if (Device->MetadataWait && !--Device->MetadataWait) {
				sq_metadata_t metadata;
				u32_t hash;
				bool valid;

				pthread_mutex_unlock(&Device->Mutex);

				valid = sq_get_metadata(Device->SqueezeHandle, &metadata, false);

				// not a valid metadata, nothing to update
				if (!valid) {
					Device->MetadataWait = 5;
					sq_free_metadata(&metadata);
					continue;
				}

				// on live stream, gather metadata every 5 seconds
				if (metadata.remote && !metadata.duration) Device->MetadataWait = 5;

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
			else pthread_mutex_unlock(&Device->Mutex);

			continue;
		}

		LOG_INFO("[%p]: request %s", Device, req->Type);

		if (!strcasecmp(req->Type, "CONNECT")) {
		}

		if (!strcasecmp(req->Type, "PAUSE")) {
		}

		if (!strcasecmp(req->Type, "STOP")) {

		}

		if (!strcasecmp(req->Type, "OFF")) {
		}

		if (!strcasecmp(req->Type, "VOLUME")) {
		}

		free(req);

	}

	return NULL;
}


/*----------------------------------------------------------------------------*/
static bool RefreshTO(char *UDN)
{
	int i;

	for (i = 0; i < MAX_RENDERERS; i++) {
		if (glHBDevices[i].InUse && !strcmp(glHBDevices[i].UDN, UDN)) {
			glHBDevices[i].TimeOut = false;
			glHBDevices[i].MissingCount = glHBDevices[i].Config.RemoveCount;
			return true;
		}
	}
	return false;
}


/*----------------------------------------------------------------------------*/
static void *UpdateHueThread(void *args)
{
	struct sHB *Device = NULL;
	int i, TimeStamp;
	static bool Running = false;
	struct sLocList *p, *m;

	if (Running) return NULL;
	Running = true;

	LOG_DEBUG("Begin hue devices update", NULL);
	TimeStamp = gettime_ms();

	// first add any newly found uPNP renderer
	ithread_mutex_lock(&glHBFoundMutex);
	m = p = glHBFoundList;
	glHBFoundList = NULL;
	ithread_mutex_unlock(&glHBFoundMutex);

	if (!glMainRunning) {
		LOG_DEBUG("Aborting ...", NULL);
		while (p) {
			m = p->Next;
			free(p->Location); free(p);
			p = m;
		}
		return NULL;
	}

	while (p) {
		IXML_Document *DescDoc = NULL;
		char *UDN = NULL, *Manufacturer = NULL, *ModelName;
		int rc;
		void *n = p->Next;

		rc = UpnpDownloadXmlDoc(p->Location, &DescDoc);
		if (rc != UPNP_E_SUCCESS) {
			LOG_DEBUG("Error obtaining description %s -- error = %d\n", p->Location, rc);
			if (DescDoc) ixmlDocument_free(DescDoc);
			p = n;
			continue;
		}

		Manufacturer = XMLGetFirstDocumentItem(DescDoc, "manufacturer");
		ModelName = XMLGetFirstDocumentItem(DescDoc, "modelName");
		UDN = XMLGetFirstDocumentItem(DescDoc, "UDN");
		if (!strstr(Manufacturer, cLogitech) && strstr(ModelName, cPhilipsHueBridge) && !RefreshTO(UDN)) {
			// new device so search a free spot.
			for (i = 0; i < MAX_RENDERERS && glHBDevices[i].InUse; i++);

			// no more room !
			if (i == MAX_RENDERERS) {
				LOG_ERROR("Too many hue devices", NULL);
				NFREE(UDN); NFREE(Manufacturer);
				break;
			}

			Device = &glHBDevices[i];
			if (AddHueDevice(Device, UDN, DescDoc, p->Location) && !glSaveConfigFile) {
                // create a new slimdevice
                Device->SqueezeHandle = sq_reserve_device(Device, &sq_callback);
                if (!*(Device->sq_config.name)) strcpy(Device->sq_config.name, Device->Hue.name);
                if (!Device->SqueezeHandle || !sq_run_device(Device->vPlayer, Device->SqueezeHandle, &Device->Hue, &Device->sq_config)) {
                    sq_release_device(Device->SqueezeHandle);
                    Device->SqueezeHandle = 0;
                    LOG_ERROR("[%p]: cannot create squeezelite instance (%s)", Device, Device->FriendlyName);
                    DelHueDevice(Device);
                }
            }
        }

		if (DescDoc) ixmlDocument_free(DescDoc);
		NFREE(UDN);	NFREE(Manufacturer);
		p = n;
	}

	// free the list of discovered location URL's
	p = m;
	while (p) {
		m = p->Next;
		free(p->Location); free(p);
		p = m;
	}

	// then walk through the list of devices to remove missing ones
	for (i = 0; i < MAX_RENDERERS; i++) {
		Device = &glHBDevices[i];
		if (!Device->InUse || !Device->Config.RemoveCount) continue;
		if (Device->TimeOut && Device->MissingCount) Device->MissingCount--;
		if (Device->Connected || Device->MissingCount) continue;

		LOG_INFO("[%p]: removing (%s)", Device, Device->FriendlyName);
		if (Device->SqueezeHandle) sq_delete_device(Device->SqueezeHandle);
		DelHueDevice(Device);
	}

	glDiscovery = true;
	if (glAutoSaveConfigFile && !glSaveConfigFile) {
		LOG_DEBUG("Updating configuration %s", glConfigName);
		SaveConfig(glConfigName, glConfigID, false);
	}

	LOG_DEBUG("End Hue devices update %d", gettime_ms() - TimeStamp);

	Running = false;
	return NULL;
}


/*----------------------------------------------------------------------------*/
static void *MainThread(void *args)
{
	unsigned last = gettime_ms();
	u32_t ScanPoll = glScanInterval*1000 + 1;

	while (glMainRunning) {
		int i;
		int elapsed = gettime_ms() - last;

		// reset timeout and re-scan devices
		ScanPoll += elapsed;
		if (glScanInterval && ScanPoll > glScanInterval*1000) {
			int rc;
			ScanPoll = 0;

			for (i = 0; i < MAX_RENDERERS; i++) {
				glHBDevices[i].TimeOut = true;
				glDiscovery = false;
			}

			// launch a new search for Media Render
			rc = UpnpSearchAsync(glControlPointHandle, glScanTimeout, SEARCH_TARGET, NULL);
			if (UPNP_E_SUCCESS != rc) LOG_ERROR("Error sending search update%d", rc);
		}

		if (glLogFile && glLogLimit != - 1) {
			s32_t size = ftell(stderr);

			if (size > glLogLimit*1024*1024) {
				u32_t Sum, BufSize = 16384;
				u8_t *buf = malloc(BufSize);

				FILE *rlog = fopen(glLogFile, "rb");
				FILE *wlog = fopen(glLogFile, "r+b");
				LOG_DEBUG("Resizing log", NULL);
				for (Sum = 0, fseek(rlog, size - (glLogLimit*1024*1024) / 2, SEEK_SET);
					 (BufSize = fread(buf, 1, BufSize, rlog)) != 0;
					 Sum += BufSize, fwrite(buf, 1, BufSize, wlog));

				Sum = fresize(wlog, Sum);
				fclose(wlog);
				fclose(rlog);
				NFREE(buf);
				if (!freopen(glLogFile, "a", stderr)) {
					LOG_ERROR("re-open error while truncating log", NULL);
				}
			}
		}

		last = gettime_ms();
		sleep(1);
	}
	return NULL;
}


/*----------------------------------------------------------------------------*/
static bool AddHueDevice(struct sHB *Device, char *UDN, IXML_Document *DescDoc, const char *location)
{
	char *deviceType = NULL;
	char *friendlyName = NULL;
	char *URLBase = NULL;
	char *presURL = NULL;
	char *manufacturer = NULL;
	pthread_attr_t attr;
	u32_t mac_size = 6;
	bool ret = true;

	// read parameters from default then config file
	memset(Device, 0, sizeof(struct sHB));
	memcpy(&Device->Config, &glHBConfig, sizeof(tHBConfig));
	memcpy(&Device->sq_config, &glDeviceParam, sizeof(sq_dev_param_t));
	LoadHBConfig(glConfigID, UDN, &Device->Config, &Device->sq_config);
	if (!Device->Config.Enabled) return false;

	// Read key elements from description document
	deviceType = XMLGetFirstDocumentItem(DescDoc, "deviceType");
	friendlyName = XMLGetFirstDocumentItem(DescDoc, "friendlyName");
	if (!friendlyName || !*friendlyName) friendlyName = strdup(UDN);
	URLBase = XMLGetFirstDocumentItem(DescDoc, "URLBase");
	presURL = XMLGetFirstDocumentItem(DescDoc, "presentationURL");
	manufacturer = XMLGetFirstDocumentItem(DescDoc, "manufacturer");

	LOG_SDEBUG("UDN:\t%s\nDeviceType:\t%s\nFriendlyName:\t%s", UDN, deviceType, friendlyName);

	if (presURL) {
		char UsedPresURL[200] = "";
		UpnpResolveURL((URLBase ? URLBase : location), presURL, UsedPresURL);
		strcpy(Device->PresURL, UsedPresURL);
	}
	else strcpy(Device->PresURL, "");

	LOG_INFO("[%p]: adding renderer (%s)", Device, friendlyName);

	Device->TimeOut = false;
	Device->Connected = true;
	Device->MissingCount = Device->Config.RemoveCount;
	Device->SqueezeHandle = 0;
	Device->Running = true;
	Device->InUse = true;
	Device->sqState = SQ_STOP;
	strcpy(Device->UDN, UDN);
	strcpy(Device->DescDocURL, location);
	strcpy(Device->FriendlyName, friendlyName);
	strcpy(Device->Manufacturer, manufacturer);

	Device->Hue.ipAddress.s_addr = ExtractIP(location);
	Device->vPlayer = virtual_create(FRAMES_PER_BLOCK);

	strcpy(Device->Hue.userName, Device->Config.UserName);

    hue_get_bridge_config(&Device->Hue);
    if (!memcmp(Device->sq_config.mac, "\0\0\0\0\0\0", mac_size)) {
        memcpy(Device->sq_config.mac, Device->Hue.mac, mac_size);
    }

	if (!hue_get_all_capabilities(&Device->Hue)) {
		LOG_SDEBUG("[%p]: connected to bridge (%s)", Device, Device->Hue.name);
        Device->UserValid = true;
    } else {
		LOG_WARN("[%p]: cannot get bridge capabilities (check username)", Device);
        Device->UserValid = false;
		ret = false;
	}

	NFREE(manufacturer)
	NFREE(deviceType);
	NFREE(friendlyName);
	NFREE(URLBase);
	NFREE(presURL);

	pthread_mutex_init(&Device->Mutex, 0);
	pthread_cond_init(&Device->Cond, 0);
	QueueInit(&Device->Queue);

	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN + 32*1024);
	pthread_create(&Device->Thread, &attr, &PlayerThread, Device);
	pthread_attr_destroy(&attr);

	return ret;
}


/*----------------------------------------------------------------------------*/
void FlushHueDevices(void)
{
	int i;

	for (i = 0; i < MAX_RENDERERS; i++) {
		struct sHB *p = &glHBDevices[i];
		if (p->InUse) DelHueDevice(p);
	}
}


/*----------------------------------------------------------------------------*/
void DelHueDevice(struct sHB *Device)
{
	pthread_mutex_lock(&Device->Mutex);
	Device->Running = false;
	Device->InUse = false;
	pthread_cond_signal(&Device->Cond);
	pthread_mutex_unlock(&Device->Mutex);

	pthread_join(Device->Thread, NULL);

	pthread_cond_destroy(&Device->Cond);
	pthread_mutex_destroy(&Device->Mutex);

	virtual_destroy(Device->vPlayer);

	LOG_INFO("[%p]: Hue device stopped", Device);

	memset(Device, 0, sizeof(struct sHB));
}


/*----------------------------------------------------------------------------*/
int uPNPSearchMediaRenderer(void)
{
	int rc;

	/* search for (Media Render and wait 15s */
	glDiscovery = false;
	rc = UpnpSearchAsync(glControlPointHandle, SCAN_TIMEOUT, SEARCH_TARGET, NULL);

	if (UPNP_E_SUCCESS != rc) {
		LOG_ERROR("Error sending uPNP search request%d", rc);
		return false;
	}
	return true;
}


/*----------------------------------------------------------------------------*/
int Start(void)
{
	int rc;
	pthread_attr_t attr;

	if (glScanInterval) {
		if (glScanInterval < SCAN_INTERVAL) glScanInterval = SCAN_INTERVAL;
		if (glScanTimeout < SCAN_TIMEOUT) glScanTimeout = SCAN_TIMEOUT;
		if (glScanTimeout > glScanInterval - SCAN_TIMEOUT) glScanTimeout = glScanInterval - SCAN_TIMEOUT;
	}

	ithread_mutex_init(&glHBFoundMutex, 0);
	memset(&glHBDevices, 0, sizeof(glHBDevices));

	UpnpSetLogLevel(UPNP_ALL);

	if (!strstr(glUPnPSocket, "?")) sscanf(glUPnPSocket, "%[^:]:%u", glIPaddress, &glPort);

	if (*glIPaddress) rc = UpnpInit(glIPaddress, glPort);
	else rc = UpnpInit(NULL, glPort);

	if (rc != UPNP_E_SUCCESS) {
		LOG_ERROR("UPnP init failed: %d\n", rc);
		UpnpFinish();
		return false;
	}

	UpnpSetMaxContentLength(60000);

	if (!*glIPaddress) strcpy(glIPaddress, UpnpGetServerIpAddress());
	if (!glPort) glPort = UpnpGetServerPort();

	if (rc != UPNP_E_SUCCESS) {
		LOG_ERROR("uPNP init failed: %d\n", rc);
		UpnpFinish();
		return false;
	}

	if (!*glIPaddress) strcpy(glIPaddress, UpnpGetServerIpAddress());
	if (!glPort) glPort = UpnpGetServerPort();

	LOG_INFO("uPNP init success - %s:%u", glIPaddress, glPort);

	rc = UpnpRegisterClient(CallbackEventHandler,
				&glControlPointHandle, &glControlPointHandle);

	if (rc != UPNP_E_SUCCESS) {
		LOG_ERROR("Error registering ControlPoint: %d", rc);
		UpnpFinish();
		return false;
	}

	/* start the main thread */
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN + 64*1024);
	pthread_create(&glMainThread, &attr, &MainThread, NULL);
	pthread_attr_destroy(&attr);

	uPNPSearchMediaRenderer();

	return true;
}


/*----------------------------------------------------------------------------*/
static bool Stop(void)
{
	struct sLocList *p, *m;

	LOG_DEBUG("flush renderers ...", NULL);
	FlushHueDevices();
	LOG_DEBUG("terminate main thread ...", NULL);
	pthread_join(glUpdateHueThread, NULL);
	pthread_join(glMainThread, NULL);
	LOG_DEBUG("un-register libupnp callbacks ...", NULL);
	UpnpUnRegisterClient(glControlPointHandle);
	LOG_DEBUG("end libupnp ...", NULL);
	UpnpFinish();

	ithread_mutex_lock(&glHBFoundMutex);
	m = p = glHBFoundList;
	glHBFoundList = NULL;
	ithread_mutex_unlock(&glHBFoundMutex);
	while (p) {
		m = p->Next;
		free(p->Location); free(p);
		p = m;
	}

	return true;
}


/*---------------------------------------------------------------------------*/
static void sighandler(int signum) {
	glMainRunning = false;

	if (!glGracefullShutdown) {
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
	int i;

#define MAXCMDLINE 256
	char cmdline[MAXCMDLINE] = "";

	for (i = 0; i < argc && (strlen(argv[i]) + strlen(cmdline) + 2 < MAXCMDLINE); i++) {
		strcat(cmdline, argv[i]);
		strcat(cmdline, " ");
	}

	while (optind < argc && strlen(argv[optind]) >= 2 && argv[optind][0] == '-') {
		char *opt = argv[optind] + 1;
		if (strstr("stxdfpib", opt) && optind < argc - 1) {
			optarg = argv[optind + 1];
			optind += 2;
		} else if (strstr("tzZIk"
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
		case 's':
			strcpy(glDeviceParam.server, optarg);
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
			} else {
				strcpy(glDeviceParam.resample_options, "");
				glDeviceParam.resample = false;
			}
			break;
#endif
		case 'f':
			glLogFile = optarg;
			break;
		case 'i':
			glSaveConfigFile = optarg;
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
			glGracefullShutdown = false;
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
					if (!strcmp(l, "all") || !strcmp(l, "slimproto"))	slimproto_loglevel = new;
					if (!strcmp(l, "all") || !strcmp(l, "stream"))    	stream_loglevel = new;
					if (!strcmp(l, "all") || !strcmp(l, "decode"))    	decode_loglevel = new;
					if (!strcmp(l, "all") || !strcmp(l, "output"))    	output_loglevel = new;
					if (!strcmp(l, "all") || !strcmp(l, "main"))     	main_loglevel = new;
					if (!strcmp(l, "all") || !strcmp(l, "util"))    	util_loglevel = new;
					if (!strcmp(l, "all") || !strcmp(l, "slimmain"))    slimmain_loglevel = new;				}
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
			strcpy(glConfigName, argv[i+1]);
		}
	}

	// load config from xml file
	glConfigID = (void*) LoadConfig(glConfigName, &glHBConfig, &glDeviceParam);

	// potentially overwrite with some cmdline parameters
	if (!ParseArgs(argc, argv)) exit(1);

	if (glLogFile) {
		if (!freopen(glLogFile, "a", stderr)) {
			fprintf(stderr, "error opening logfile %s: %s\n", glLogFile, strerror(errno));
		}
	}

	LOG_ERROR("Starting squeeze2hue version: %s\n", VERSION);

	if (!glConfigID) {
		LOG_ERROR("\n\n!!!!!!!!!!!!!!!!!! ERROR LOADING CONFIG FILE !!!!!!!!!!!!!!!!!!!!!\n", NULL);
	}

#if LINUX || FREEBSD || SUNOS
	if (glDaemonize && !glSaveConfigFile) {
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

	sq_init();

	if (!Start()) {
		LOG_ERROR("Cannot start", NULL);
		strcpy(resp, "exit");
	}

	if (glSaveConfigFile) {
		while (!glDiscovery) sleep(1);
		SaveConfig(glSaveConfigFile, glConfigID, true);
	}

	while (strcmp(resp, "exit") && !glSaveConfigFile) {

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

	if (!strcmp(resp, "streamdbg"))	{
			char level[20];
			i = scanf("%s", level);
			stream_loglevel = debug2level(level);
		}

		if (!strcmp(resp, "outputdbg"))	{
			char level[20];
			i = scanf("%s", level);
			output_loglevel = debug2level(level);
		}

		if (!strcmp(resp, "decodedbg"))	{
			char level[20];
			i = scanf("%s", level);
			decode_loglevel = debug2level(level);
		}

		if (!strcmp(resp, "slimprotodbg"))	{
			char level[20];
			i = scanf("%s", level);
			slimproto_loglevel = debug2level(level);
		}

		if (!strcmp(resp, "maindbg"))	{
			char level[20];
			i = scanf("%s", level);
			main_loglevel = debug2level(level);
		}

		if (!strcmp(resp, "slimmainqdbg"))	{
			char level[20];
			i = scanf("%s", level);
			slimmain_loglevel = debug2level(level);
		}

		if (!strcmp(resp, "utildbg"))	{
			char level[20];
			i = scanf("%s", level);
			util_loglevel = debug2level(level);
		}

		 if (!strcmp(resp, "save"))	{
			char name[128];
			i = scanf("%s", name);
			SaveConfig(name, glConfigID, true);
		}
	}

	if (glConfigID) ixmlDocument_free(glConfigID);
	glMainRunning = false;
	LOG_INFO("stopping squeelite devices ...", NULL);
	sq_end();
	LOG_INFO("stopping Raop devices ...", NULL);
	Stop();
	LOG_INFO("all done", NULL);

	return true;
}




