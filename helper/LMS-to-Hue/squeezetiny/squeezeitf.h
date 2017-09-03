/* 
 *  Squeezelite - lightweight headless squeezebox emulator
 *
 *  (c) Adrian Smith 2012-2014, triode1@btinternet.com
 *  (c) Philippe, philippe_44@outlook.com for raop/multi-instance modifications
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

#ifndef __SQUEEZEITF_H
#define __SQUEEZEITF_H

#include "squeezedefs.h"
#include "util_common.h"

#define SQ_STR_LENGTH	256

typedef enum { SQ_NONE, SQ_PLAY, SQ_PAUSE, SQ_UNPAUSE, SQ_STOP, SQ_SEEK,
			  SQ_VOLUME, SQ_ONOFF, SQ_NEXT, SQ_CONNECT, SQ_STARTED,
			  SQ_METASEND, SQ_SETNAME, SQ_SETSERVER, SQ_FINISHED, SQ_PLAY_PAUSE,
			  SQ_MUTE_TOGGLE, SQ_PREVIOUS, SQ_SHUFFLE,
			  SQ_FF_REW } sq_action_t;
typedef	sq_action_t sq_event_t;


#define FRAMES_PER_BLOCK			352
#define MAX_SUPPORTED_SAMPLERATES 	2

#define STREAMBUF_SIZE 				(2 * 1024 * 1024)
#define OUTPUTBUF_SIZE 				(44100 * 4 * 10)

typedef	int	sq_dev_handle_t;
typedef unsigned sq_rate_t;

typedef struct sq_metadata_s {
	char *artist;
	char *album;
	char *title;
	char *genre;
	char *path;
	char *artwork;
	u32_t index;
	u32_t track;
	u32_t duration;
	u32_t file_size;
	bool  remote;
	u32_t track_hash;
} sq_metadata_t;

typedef	struct sq_dev_param_s {
	unsigned 	stream_buf_size;
	unsigned 	output_buf_size;
	char		codecs[SQ_STR_LENGTH];
	char 		server[SQ_STR_LENGTH];
	char 		name[SQ_STR_LENGTH];
	u8_t		mac[6];
	u32_t		sample_rate;
#if defined(RESAMPLE)
	bool		resample;
	char		resample_options[SQ_STR_LENGTH];
#endif
	struct {
		char 	server[SQ_STR_LENGTH];
	} dynamic;
} sq_dev_param_t;


typedef struct
{
	u8_t	channels;
	u8_t	sample_size;
	u32_t	sample_rate;
	u8_t	codec;
	u8_t 	endianness;
} sq_format_t;


typedef struct hue_bridge_s hue_bridge_t;

typedef bool (*sq_callback_t)(sq_dev_handle_t handle, void *caller_id, sq_action_t action, void *param);

void				sq_init(void);
void				sq_end(void);

bool			 	sq_run_device(sq_dev_handle_t handle, hue_bridge_t *hue, sq_dev_param_t *param);
void				sq_delete_device(sq_dev_handle_t);
sq_dev_handle_t		sq_reserve_device(void *caller_id, sq_callback_t callback);
void				sq_release_device(sq_dev_handle_t);

bool				sq_call(sq_dev_handle_t handle, sq_action_t action, void *param);
void				sq_notify(sq_dev_handle_t handle, void *caller_id, sq_event_t event, u8_t *cookie, void *param);
bool				sq_get_metadata(sq_dev_handle_t handle, struct sq_metadata_s *metadata, bool next);
void				sq_default_metadata(struct sq_metadata_s *metadata, bool init);
void 				sq_free_metadata(struct sq_metadata_s *metadata);
u32_t 				sq_get_time(sq_dev_handle_t handle);
sq_action_t 		sq_get_mode(sq_dev_handle_t handle);

#endif

