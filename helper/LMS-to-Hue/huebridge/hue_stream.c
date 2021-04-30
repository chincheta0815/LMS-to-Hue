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

#include "platform.h"

#include <pthread.h>

#include "log_util.h"

#include "hue_bridge.h"
#include "dtls.h"
#include "hue_analyze.h"

extern log_level    huestream_loglevel;
static log_level    *loglevel = &huestream_loglevel;

void huebridge_stream_log_callback(const char* message, void *user_data) {
    logprint("%s %s %s\n", logtime(), "    hue_stream_cb", message);
    return;
}

static void *HueStreamThread(void *args) {
    struct huebridgecl_s *p = (struct huebridgecl_s*) args;
    int interval_us;
    void *msg_buf;
    int buf_len;

    LOG_DEBUG("[%p]: connecting to Hue Entertainment Bridge via DTLS", p);
    if (dtls_connect(&p->hue_dtls_ctx, p->hue_rest_ctx.address, HUE_API_DTLS_PORT)) {
        LOG_ERROR("[%p]: failed to establish DTLS connection to Hue Entertainment Bridge", p);
        dtls_cleanup(&p->hue_dtls_ctx);
        hue_ent_cleanup(&p->hue_ent_ctx);
        pthread_mutex_unlock(&p->Mutex);
        return false;
    }

    interval_us = 1000000 / HUE_ENTERTAINMENT_FRAMERATE;

    while(p->stream_thread_running) {

        usleep(interval_us);

        if (p->StreamState >= HUE_STREAM_WAITING) {
            pthread_mutex_lock(&p->Mutex);
            hue_ent_get_message(&p->hue_ent_ctx, &msg_buf, &buf_len);
            if(dtls_send_data(&p->hue_dtls_ctx, msg_buf, buf_len)) {
                LOG_DEBUG("[%p]: DTLS stream connection lost", p);
            }
            pthread_mutex_unlock(&p->Mutex);
        }
        else {
            LOG_SDEBUG("[%p]: nothing incoming", p);
        }

    }

    return NULL;
}

bool hue_ent_stream_init(struct huebridgecl_s *p) {
    int i;
    debug_cb_t huebridge_stream_callback;

    huebridge_stream_callback = huebridge_stream_log_callback;

    int hue_dtls_loglevel = 0;

    if (*loglevel >= lDEBUG)
        hue_dtls_loglevel = MSG_DEBUG;
    if (*loglevel == lERROR)
        hue_dtls_loglevel = MSG_ERR;
    if (*loglevel == lWARN)
        hue_dtls_loglevel = MSG_INFO;
    if (*loglevel == lINFO)
        hue_dtls_loglevel = MSG_INFO;
    if (*loglevel == lSILENCE)
        hue_dtls_loglevel = MSG_OFF;

    LOG_DEBUG("[%p]: enabling entertainment area %s for streaming", p, p->hue_rest_ctx.ent_areas->area_name);
    hue_rest_activate_stream(&p->hue_rest_ctx, p->hue_rest_ctx.ent_areas->area_id);

    /*
      Big assumption: light IDs are in an order that makes sense (e.g. left to right).
      This probably isn't the case. TODO: the brige does provide x/y position information
      for the bulbs, so should use that to figure out a better order
    */
    if(hue_ent_init(&p->hue_ent_ctx, p->light_count)) {
        LOG_ERROR("[%p]: could not initialize entertainment context", p);
        pthread_mutex_unlock(&p->Mutex);
        return false;
    }

    for (i = 0; i < p->light_count; i++) {
        if (hue_ent_set_light_id(&p->hue_ent_ctx, i, p->hue_rest_ctx.ent_areas->light_ids[i])) {
            LOG_ERROR("[%p]: could not set light id for entertainment context", p);
            hue_ent_cleanup(&p->hue_ent_ctx);
            pthread_mutex_unlock(&p->Mutex);
            return false;
        }
    }

    LOG_DEBUG("[%p]: initializing DTLS context for connection to Hue Entertainment Bridge %s", p, p->hue_rest_ctx.address);
    LOG_SDEBUG("[%p]:    username %s, clientkey %s", p, p->hue_rest_ctx.username, p->hue_rest_ctx.clientkey);
    if (dtls_init(&p->hue_dtls_ctx, p->hue_rest_ctx.username, p->hue_rest_ctx.clientkey, huebridge_stream_callback, hue_dtls_loglevel)) {
        LOG_ERROR("[%p]: could not create DTLS connection context", p);
        pthread_mutex_unlock(&p->Mutex);
        return false;
    }

    pthread_mutex_unlock(&p->Mutex);

    p->stream_thread_running = true;
    pthread_create(&p->stream_thread, NULL, HueStreamThread, (void*) p);

    return true;
}

void hue_ent_stream_stop(struct huebridgecl_s *p) {
    p->stream_thread_running = false;
    pthread_join(p->stream_thread, NULL);

    //dtls_cleanup(&p->hue_dtls_ctx);
    //hue_ent_cleanup(&p->hue_ent_ctx);

}
