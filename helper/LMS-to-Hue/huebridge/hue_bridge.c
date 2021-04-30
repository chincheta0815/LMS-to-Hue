/*
 *				 2016 Philippe <philippe_44@outlook.com>
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
#include "platform.h"
#include <time.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include <pthread.h>
#include <semaphore.h>

#include "util_common.h"
#include "log_util.h"

#include "hue_bridge.h"
#include "hue_stream.h"
#include "hue_analyze.h"

#ifdef SEC
#undef SEC
#endif
#define SEC(ntp) ((u32_t) ((ntp) >> 32))
#define FRAC(ntp) ((u32_t) (ntp))
#define SECNTP(ntp) SEC(ntp),FRAC(ntp)
#define MSEC(ntp)  ((u32_t) ((((ntp) >> 16)*1000) >> 16))

extern log_level    huebridge_loglevel;
static log_level    *loglevel = &huebridge_loglevel;

/*----------------------------------------------------------------------------*/
typedef void (*debug_cb_t)(const char *message, void *user_data);

void huebridge_rest_log_callback(const char* message, void *user_data) {
    logprint("%s %s %s\n", logtime(), "    hue_rest_cb", message);
    return;
}

/*----------------------------------------------------------------------------*/
u64_t huebridge_time32_to_ntp(u32_t time) {
    u64_t ntp_ms = ((get_ntp(NULL) >> 16) * 1000) >> 16;
    u32_t ms = (u32_t) ntp_ms;
    u64_t res;

    /*
     Received time is supposed to be derived from an NTP in a form of
     (NTP.second * 1000 + NTP.fraction / 1000) & 0xFFFFFFFF
     with many rollovers as NTP started in 1900. It's also assumed that "time"
     is not older then 60 seconds
    */
    if (ms > time + 60000 || ms + 60000 < time) ntp_ms += 0x100000000LL;

    res = ((((ntp_ms & 0xffffffff00000000LL) | time) << 16) / 1000) << 16;

    return res;
}


/*----------------------------------------------------------------------------*/
void huebridge_pause(struct huebridgecl_s *p) {
    if (!p || p->StreamState != HUE_STREAM_ACTIVE)
        return;

    pthread_mutex_lock(&p->Mutex);
    p->pause_ts = p->head_ts;
    p->waiting = true;
    p->StreamState = HUE_STREAM_WAITING;
    pthread_mutex_unlock(&p->Mutex);

    LOG_INFO("[%p]: set pause %Lu", p, p->pause_ts);
}


/*----------------------------------------------------------------------------*/
bool huebridge_start_at(struct huebridgecl_s *p, u64_t start_time) {
    if (!p) return
        false;

    pthread_mutex_lock(&p->Mutex);
    p->start_ts = NTP2TS(start_time, 44100);
    pthread_mutex_unlock(&p->Mutex);

    LOG_INFO("[%p]: set start time %u.%u (ts:%Lu)", p, SEC(start_time), FRAC(start_time), p->start_ts);

    return true;
}


/*----------------------------------------------------------------------------*/
void huebridge_stop(struct huebridgecl_s *p) {
    if (!p)
        return;

    hue_ent_stream_stop(p);

    pthread_mutex_lock(&p->Mutex);
    p->waiting = true;
    p->pause_ts = 0;
    p->ConnectionState = HUE_STREAM_DISCONNECTED;
    pthread_mutex_unlock(&p->Mutex);
}


/*----------------------------------------------------------------------------*/
bool huebridge_accept_frames(struct huebridgecl_s *p) {
    bool accept = false;
    u64_t now_ts;

    if (!p)
        return false;

    pthread_mutex_lock(&p->Mutex);

    // not active yet
    if (p->waiting) {
        u64_t now = get_ntp(NULL);

        now_ts = NTP2TS(now, 44100);

        // Not flushed yet, but we have time to wait, so pretend we are full
        if (p->StreamState != HUE_STREAM_WAITING && (!p->start_ts || p->start_ts > now_ts)) {
            pthread_mutex_unlock(&p->Mutex);

            return false;
        }

        // move to streaming only when really flushed - not when timedout
        if (p->StreamState == HUE_STREAM_WAITING) {
            LOG_INFO("[%p]: beginning to stream hts:%Lu n:%u.%u", p, p->head_ts, SECNTP(now));
            p->StreamState = HUE_STREAM_ACTIVE;
        }

        // unpausing ...
        if (!p->pause_ts) {
            p->head_ts = p->first_ts = p->start_ts ? p->start_ts : now_ts;
            LOG_INFO("[%p]: restarting w/o pause n:%u.%u, hts:%Lu", p, SECNTP(now), p->head_ts);

        }
        else {
            // if un-pausing w/o start_time, can anticipate as we have buffer
            p->first_ts = p->start_ts ? p->start_ts : now_ts;

            // last head_ts shall be first
            p->head_ts = p->first_ts - p->chunk_len;

            LOG_INFO("[%p]: restarting w/ pause n:%u.%u, hts:%Lu", p, SECNTP(now), p->head_ts);
        }

        p->pause_ts = p->start_ts = 0;
        p->waiting = false;
    }

    // when paused, fix "now" at the time when it was paused.
    if (p->pause_ts) now_ts = p->pause_ts;
    else now_ts = NTP2TS(get_ntp(NULL), 44100);

    if (now_ts >= p->head_ts + p->chunk_len) accept = true;

    pthread_mutex_unlock(&p->Mutex);

    return accept;
}


/*----------------------------------------------------------------------------*/
bool huebridge_process_chunk(struct huebridgecl_s *p, s16_t *sample, int frames, u64_t *playtime) {
    int i;
    s16_t channel_left[frames/2];
    s16_t channel_right[frames/2];
    audio_type_t audio_type;

    p->flash_mode = 0;

    if (!p || !sample) {
        LOG_ERROR("[%p]: something went wrong (s:%p)", p, sample);

        return false;
    }

    pthread_mutex_lock(&p->Mutex);

    *playtime = TS2NTP(p->head_ts, 44100);
    p->head_ts += p->chunk_len;

    //LOG_SDEBUG("[%p]: chunk_len %d, samples %d", p, p->chunk_len, frames);

    audio_type = STEREO;
    if (audio_type == MONO) {
        for (i = 0; i < (frames / 2); i++) {
            channel_left[i] = (sample[i] / 2) + (sample[i+1] / 2);
            channel_right[i] = channel_left[i];
            LOG_SDEBUG("[%p]: channel left (%d), channel right (%d)", p, channel_left[i], channel_right[i]);
        }
    }
    else if (audio_type == STEREO) {
        for (i = 0; i < (frames / 2); i++) {
            channel_left[i] = sample[i];
            channel_right[i] = sample[i+1];
            LOG_SDEBUG("[%p]: channel left (%d), channel right (%d)", p, channel_left[i], channel_right[i]);
        }
    }

    hue_analyze_audio(p, channel_left, channel_right);

    pthread_mutex_unlock(&p->Mutex);

    if (NTP2MS(*playtime) % 10000 < 8) {
        u64_t now = get_ntp(NULL);
        LOG_INFO("[%p]: check n:%u p:%u ts:%Lu", p, MSEC(now), MSEC(*playtime), p->head_ts);
    }

    return true;
}

/*----------------------------------------------------------------------------*/
bool huebridge_rest_init() {
    return hue_rest_init();
}

/*----------------------------------------------------------------------------*/
void huebridge_rest_cleanup() {
    hue_rest_cleanup();
}

/*----------------------------------------------------------------------------*/
struct huebridgecl_s *huebridge_create(struct in_addr ipAddress, char *username, char *clientkey, int chunk_len) {
    debug_cb_t huebridge_rest_callback;
    huebridge_rest_callback = huebridge_rest_log_callback;

    int hue_rest_loglevel;
    struct huebridgecl_s *huebridgecld;

    if (chunk_len > MAX_SAMPLES_PER_CHUNK) {
        LOG_ERROR("chunk length must be below %d", MAX_SAMPLES_PER_CHUNK);
        return NULL;
    }

    huebridgecld = malloc(sizeof(struct huebridgecl_s));
    memset(huebridgecld, 0, sizeof(struct huebridgecl_s));

    hue_rest_loglevel = 0;
    if (*loglevel >= lDEBUG)
        hue_rest_loglevel = MSG_DEBUG;
    if (*loglevel == lERROR)
        hue_rest_loglevel = MSG_ERR;
    if (*loglevel == lWARN)
        hue_rest_loglevel = MSG_INFO;
    if (*loglevel == lINFO)
        hue_rest_loglevel = MSG_INFO;
    if (*loglevel == lSILENCE)
        hue_rest_loglevel = MSG_OFF;

    if (hue_rest_init_ctx(&huebridgecld->hue_rest_ctx, huebridge_rest_callback, inet_ntoa(ipAddress), HUE_API_SSL_PORT, username, hue_rest_loglevel)) {
        LOG_ERROR("[%p]: failed to initialize hue rest context for bridge %s", huebridgecld, inet_ntoa(ipAddress));
        return NULL;
    }

    huebridgecld->hue_rest_ctx.clientkey = clientkey;

    LOG_DEBUG("[%p]: initialized hue rest context for bridge %s", huebridgecld, inet_ntoa(ipAddress));

    if (hue_rest_validate_apiversion(&huebridgecld->hue_rest_ctx) < 0) {
        LOG_DEBUG("[%p]: hue entertainment bridge has incompatible apiversion %s", huebridgecld, huebridgecld->hue_rest_ctx.apiversion);

        free(huebridgecld);
        return NULL;
    }
    LOG_DEBUG("[%p]: hue entertainment bridge (%s) has api version %s", huebridgecld, huebridgecld->hue_rest_ctx.address, huebridgecld->hue_rest_ctx.apiversion);

    pthread_mutex_init(&huebridgecld->Mutex, NULL);

    huebridgecld->chunk_len = chunk_len;

    huebridge_sanitize(huebridgecld);

    return huebridgecld;
}

/*----------------------------------------------------------------------------*/
bool huebridge_destroy(struct huebridgecl_s *p) {
    if (!p)
        return false;

    if (!huebridge_disconnect(p))
        return false;

    // does not seem to be needed. valgrind does not detect a leak.
    //hue_rest_cleanup_ctx(&p->hue_rest_ctx);

    pthread_mutex_destroy(&p->Mutex);

    free(p);

    return true;
}

/*----------------------------------------------------------------------------*/
bool huebridge_sanitize(struct huebridgecl_s *p) {
    if(!p)
        return false;

    pthread_mutex_trylock(&p->Mutex);

    p->ConnectionState = HUE_STREAM_DISCONNECTED;
    p->head_ts = p->pause_ts = p->start_ts = p->first_ts = 0;
    p->waiting = true;

    pthread_mutex_unlock(&p->Mutex);

    return true;
}

/*----------------------------------------------------------------------------*/
bool huebridge_check_prerequisites(struct huebridgecl_s *p) {

    struct hue_entertainment_area *out_ent_areas;
    int out_ent_areas_count;

    hue_rest_get_ent_groups(&p->hue_rest_ctx, &out_ent_areas, &out_ent_areas_count);
    if (out_ent_areas_count <= 0) {
        LOG_DEBUG("[%p]: no entertainment areas found", p);
        return false;
    }
    else {
        LOG_DEBUG("[%p]: found %d entertainment areas", p, out_ent_areas_count);
    }

    for (p->light_count = 0; p->light_count < MAX_LIGHTS_PER_AREA; p->light_count++) {
        if (out_ent_areas->light_ids[p->light_count] == 0)
            break;
    }

    if (p->light_count == 0) {
        LOG_DEBUG("[%p]: no lights found in entertainment area (%s)", p, p->hue_rest_ctx.ent_areas->area_name);
        return false;
    }
    else {
        LOG_DEBUG("[%p]: found %d lights in entertainment area %s", p, p->light_count, p->hue_rest_ctx.ent_areas->area_name);
    }

    return true;
}

/*----------------------------------------------------------------------------*/
bool huebridge_register(struct huebridgecl_s *p) {
    LOG_INFO("[%p]: registering to hue entertainment bridge (%s)", p, p->hue_rest_ctx.address);

    char *userName;
    char *clientKey;

    LOG_INFO("[%p]: push button within 30 seconds from now ...", p);
    for (int i = 5; i > 0; i--) {
        sleep(5);
        LOG_INFO("[%p]:     remaining %d seconds", p, (i*5));
    }

    if (hue_rest_register(&p->hue_rest_ctx, &userName, &clientKey)) {
        LOG_ERROR("[%p]: failed to register to hue entertainment bridge (%s)", p, p->hue_rest_ctx.address);
        return false;
    }

    LOG_DEBUG("[%p]: register device with username %s and clientkey %s", p, p->hue_rest_ctx.username, p->hue_rest_ctx.clientkey);

    return true;
}

/*----------------------------------------------------------------------------*/
bool huebridge_connect(struct huebridgecl_s *p) {
    if (!p)
        return false;

    if (p->ConnectionState == HUE_STREAM_CONNECTED)
        return true;

    // as connect might take time, state might already have been set
    pthread_mutex_lock(&p->Mutex);

    if (p->ConnectionState == HUE_STREAM_DISCONNECTED) {
        if(!hue_ent_stream_init(p)) {
            pthread_mutex_unlock(&p->Mutex);
            return false;
        }
        p->ConnectionState = HUE_STREAM_CONNECTED;
        p->StreamState = HUE_STREAM_WAITING;
    }

    pthread_mutex_unlock(&p->Mutex);

    return true;
}

/*----------------------------------------------------------------------------*/
bool huebridge_is_connected(struct huebridgecl_s *p) {
    return true;
}

/*----------------------------------------------------------------------------*/
bool huebridge_set_volume(struct huebridgecl_s *p, float vol) {
    char a[128];

    if(!p)
        return false;

    if ((vol < -30 || vol > 0) && vol != -144.0 )
        return false;

    p->Volume = vol;

    sprintf(a, "volume: %fi\r\n", vol);

    return true;
}

/*----------------------------------------------------------------------------*/
bool huebridge_disconnect(struct huebridgecl_s *p) {
    if (!p || p->ConnectionState == HUE_STREAM_DISCONNECTED)
        return true;

    hue_ent_stream_stop(p);

    pthread_mutex_lock(&p->Mutex);
    p->ConnectionState = HUE_STREAM_DISCONNECTED;
    p->StreamState = HUE_STREAM_WAITING;
    pthread_mutex_unlock(&p->Mutex);

    return true;
}
