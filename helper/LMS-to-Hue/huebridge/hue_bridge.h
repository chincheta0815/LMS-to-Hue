/*
 * virtual.h: Virtual player
 *
 * Copyright (C) 2004 Shiro Ninomiya <shiron@snino.com>
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

#ifndef __HUE_BRIDGE_H_
#define __HUE_BRIDGE_H_

#include "platform.h"
#include "hue_rest.h"
#include "hue_entertainment.h"
#include "dtls.h"

#include "hue_analyze.h"

#define HUE_API_SSL_PORT  443
#define HUE_API_DTLS_PORT 2100
#define HUE_APP_REGISTER_TIME 30

#define HUE_ENTERTAINMENT_FRAMERATE 50

#define MAX_SAMPLES_PER_CHUNK 4096

#define NTP2MS(ntp) ((((ntp) >> 10) * 1000L) >> 22)
#define MS2NTP(ms) (((((u64_t) (ms)) << 22) / 1000) << 10)
#define TIME_MS2NTP(time) huebridge_time32_to_ntp(time)
#define NTP2TS(ntp, rate) ((((ntp) >> 16) * (rate)) >> 16)
#define TS2NTP(ts, rate)  (((((u64_t) (ts)) << 16) / (rate)) << 16)
#define MS2TS(ms, rate) ((((u64_t) (ms)) * (rate)) / 1000)
#define TS2MS(ts, rate) NTP2MS(TS2NTP(ts,rate))

typedef enum huebridge_states_s {
        HUE_STREAM_DISCONNECTED = 0,
        HUE_STREAM_CONNECTED = 1,
        HUE_STREAM_WAITING = 2,
        HUE_STREAM_ACTIVE = 3
} huebridge_state_t;

typedef struct huebridgecl_s {
    huebridge_state_t ConnectionState;
    huebridge_state_t StreamState;
    u64_t head_ts, pause_ts, start_ts, first_ts;
    float Volume;
    bool waiting;
    int chunk_len;
    pthread_mutex_t Mutex;
    int light_count;
    pthread_t stream_thread;
    bool stream_thread_running;
    flash_mode_t flash_mode;
    struct hue_rest_ctx hue_rest_ctx;
    struct hue_ent_ctx hue_ent_ctx;
    struct dtls_ctx hue_dtls_ctx;
    struct hue_analyze_ctx hue_analyze_ctx;
} huebridgecl_data_t;

struct  huebridgecl_s *huebridge_create(struct in_addr ipAddress, char *username, char *clientkey, int chunk_len);
bool    huebridge_rest_init();
void    huebridge_rest_cleanup();

bool    huebridge_register(struct huebridgecl_s *p);
bool    huebridge_check_prerequisites(struct huebridgecl_s *p);
bool    huebridge_destroy(struct huebridgecl_s *p);
bool    huebridge_connect(struct huebridgecl_s *p);
bool    huebridge_is_connected(struct huebridgecl_s *p);
bool    huebridge_disconnect(struct huebridgecl_s *p);
bool    huebridge_sanitize(struct huebridgecl_s *p);

bool    huebridge_set_volume(struct huebridgecl_s *p, float vol);

bool    huebridge_accept_frames(struct huebridgecl_s *p);
bool    huebridge_process_chunk(struct huebridgecl_s *p, s16_t *sample, int size, u64_t *playtime);

bool    huebridge_start_at(struct huebridgecl_s *p, u64_t start_time);
void    huebridge_pause(struct huebridgecl_s *p);
void    huebridge_stop(struct huebridgecl_s *p);

u64_t   huebridge_time32_to_ntp(u32_t time);

#endif
