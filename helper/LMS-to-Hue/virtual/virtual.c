/*****************************************************************************
 * virtual.c: Virtual player
 *
 * Copyright (C) 2004 Shiro Ninomiya <shiron@snino.com>
 *				 2016 Philippe <philippe_44@outlook.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/
#include <stdio.h>
#include "platform.h"
#include <time.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include <pthread.h>
#include <semaphore.h>

#include "log_util.h"
#include "virtual.h"

#include "aubio.h"
#include "libhuec.h"

#undef SEC
#define SEC(ntp) ((__u32) ((ntp) >> 32))
#define FRAC(ntp) ((__u32) (ntp))
#define SECNTP(ntp) SEC(ntp),FRAC(ntp)
#define MSEC(ntp)  ((__u32) ((((ntp) >> 16)*1000) >> 16))

typedef struct virtualcl_s {
	virtual_state_t state;
	__u64 head_ts, pause_ts, start_ts, first_ts;
	bool waiting;
	int chunk_len;
	pthread_mutex_t mutex;
} virtualcl_data_t;

extern log_level	virtual_loglevel;
static log_level 	*loglevel = &virtual_loglevel;

aubio_tempo_t *aubio_tempo;
fvec_t *aubio_tempo_in;
fvec_t *aubio_tempo_out;

/*----------------------------------------------------------------------------*/
__u64 virtual_time32_to_ntp(__u32 time)
{
	__u64 ntp_ms = ((get_ntp(NULL) >> 16) * 1000) >> 16;
	__u32 ms = (__u32) ntp_ms;
	__u64 res;

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
void virtual_pause(struct virtualcl_s *p)
{
	if (!p || p->state != STREAMING) return;

	pthread_mutex_lock(&p->mutex);
	p->pause_ts = p->head_ts;
	p->waiting = true;
	p->state = FLUSHED;
	pthread_mutex_unlock(&p->mutex);

	LOG_INFO("[%p]: set pause %Lu", p, p->pause_ts);
}


/*----------------------------------------------------------------------------*/
bool virtual_start_at(struct virtualcl_s *p, __u64 start_time)
{
	if (!p) return false;

	pthread_mutex_lock(&p->mutex);
	p->start_ts = NTP2TS(start_time, 44100);
	pthread_mutex_unlock(&p->mutex);

	LOG_INFO("[%p]: set start time %u.%u (ts:%Lu)", p, SEC(start_time), FRAC(start_time), p->start_ts);

	return true;
}


/*----------------------------------------------------------------------------*/
void virtual_stop(struct virtualcl_s *p)
{
	if (!p) return;

	pthread_mutex_lock(&p->mutex);
	p->waiting = true;
	p->pause_ts = 0;
	p->state = FLUSHED;
	pthread_mutex_unlock(&p->mutex);
}


/*----------------------------------------------------------------------------*/
bool virtual_accept_frames(struct virtualcl_s *p)
{
	bool accept = false;
	__u64 now_ts;

	if (!p) return false;

	pthread_mutex_lock(&p->mutex);

	// not active yet
	if (p->waiting) {
		__u64 now = get_ntp(NULL);

		now_ts = NTP2TS(now, 44100);

		// Not flushed yet, but we have time to wait, so pretend we are full
		if (p->state != FLUSHED && (!p->start_ts || p->start_ts > now_ts)) {
			pthread_mutex_unlock(&p->mutex);
			return false;
		 }

		// move to streaming only when really flushed - not when timedout
		if (p->state == FLUSHED) {
			LOG_INFO("[%p]: begining to stream hts:%Lu n:%u.%u", p, p->head_ts, SECNTP(now));
			p->state = STREAMING;
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

	pthread_mutex_unlock(&p->mutex);

	return accept;
}


/*---------------------------------------------------------------------------*/
int disco_process_chunk(void *device, __u8 *frame_buf, int num_frames){
    LOG_INFO("[%p]: analyzing chunk with %d frames for disco", device, num_frames);

    hue_bridge_t *bridge;
    bridge = device;

    for (int i = 0; i < num_frames; i++) {
        aubio_tempo_in->data[i] =  (smpl_t)frame_buf[i];
    }

    aubio_tempo_do(aubio_tempo, aubio_tempo_in, aubio_tempo_out);

    if(aubio_tempo_out->data[0] != 0) {
        hue_light_t hueLight;
        hueLight.attribute.id = 2;
        hue_set_light_state(bridge, &hueLight, BRI, "255");
    }
    else {
        hue_light_t hueLight;
        hueLight.attribute.id = 2;
        hue_set_light_state(bridge, &hueLight, BRI, "0");
    }

    return 0;
}


/*----------------------------------------------------------------------------*/
bool virtual_send_chunk(struct virtualcl_s *p, void *device, __u8 *sample, int frames, __u64 *playtime)
{
	if (!p || !sample) {
		LOG_ERROR("[%p]: something went wrong (s:%p)", p, sample);
		return false;
	}

	pthread_mutex_lock(&p->mutex);
	*playtime = TS2NTP(p->head_ts, 44100);
	p->head_ts += p->chunk_len;
	pthread_mutex_unlock(&p->mutex);

	if (NTP2MS(*playtime) % 10000 < 8) {
		__u64 now = get_ntp(NULL);
		LOG_INFO("[%p]: check n:%u p:%u ts:%Lu", p, MSEC(now), MSEC(*playtime), p->head_ts);
	}

    disco_process_chunk(device, sample, frames);

	return true;

}

/*----------------------------------------------------------------------------*/
struct virtualcl_s *virtual_create(int chunk_len)
{
	virtualcl_data_t *virtualcld;

	if (chunk_len > MAX_SAMPLES_PER_CHUNK) {
		LOG_ERROR("Chunk length must below %d", MAX_SAMPLES_PER_CHUNK);
		return NULL;
	}

	virtualcld = malloc(sizeof(virtualcl_data_t));
	memset(virtualcld, 0, sizeof(virtualcl_data_t));

	pthread_mutex_init(&virtualcld->mutex, NULL);

    virtualcld->chunk_len = chunk_len;
	virtualcld->state = DOWN;
	virtualcld->head_ts = virtualcld->pause_ts = virtualcld->start_ts = virtualcld->first_ts = 0;
	virtualcld->waiting = true;

	return virtualcld;
}


/*----------------------------------------------------------------------------*/
bool virtual_connect(struct virtualcl_s *p)
{
	if (!p) return false;

	if (p->state >= FLUSHED) return true;
	pthread_mutex_lock(&p->mutex);
	if (p->state == DOWN) p->state = FLUSHED;
	pthread_mutex_unlock(&p->mutex);

	return true;

}



/*----------------------------------------------------------------------------*/
bool virtual_disconnect(struct virtualcl_s *p)
{
	if (!p || p->state == DOWN) return true;

	pthread_mutex_lock(&p->mutex);
	p->state = DOWN;
	pthread_mutex_unlock(&p->mutex);

	return true;
}


/*----------------------------------------------------------------------------*/
bool virtual_destroy(struct virtualcl_s *p)
{
	if (!p) return false;

	pthread_mutex_destroy(&p->mutex);

	free(p);

	return true;
}

