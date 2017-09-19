/*****************************************************************************
 * rtsp_client.h: RAOP Client
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

#ifndef __VIRTUAL_H_
#define __VIRTUAL_H_


#include "platform.h"

#define MAX_SAMPLES_PER_CHUNK 	352

#define NTP2MS(ntp) ((((ntp) >> 10) * 1000L) >> 22)
#define MS2NTP(ms) (((((__u64) (ms)) << 22) / 1000) << 10)
#define TIME_MS2NTP(time) virtual_time32_to_ntp(time)
#define NTP2TS(ntp, rate) ((((ntp) >> 16) * (rate)) >> 16)
#define TS2NTP(ts, rate)  (((((__u64) (ts)) << 16) / (rate)) << 16)
#define MS2TS(ms, rate) ((((__u64) (ms)) * (rate)) / 1000)
#define TS2MS(ts, rate) NTP2MS(TS2NTP(ts,rate))


struct virtualcl_s;

typedef enum virtual_states_s { DOWN = 0, FLUSHED, STREAMING } virtual_state_t;

struct  virtualcl_s *virtual_create(int chunk_len);
bool	virtual_destroy(struct virtualcl_s *p);
bool	virtual_connect(struct virtualcl_s *p);
bool 	virtual_disconnect(struct virtualcl_s *p);

bool 	virtual_accept_frames(struct virtualcl_s *p);
bool	virtual_send_chunk(struct virtualcl_s *p, __u8 *sample, int size, __u64 *playtime);

bool 	virtual_start_at(struct virtualcl_s *p, __u64 start_time);
void 	virtual_pause(struct virtualcl_s *p);
void 	virtual_stop(struct virtualcl_s *p);

__u64 	virtual_time32_to_ntp(__u32 time);

#endif
