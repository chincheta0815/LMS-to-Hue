/*
 *  Squeezelite - lightweight headless squeezebox emulator
 *
 *  (c) Adrian Smith 2012-2015, triode1@btinternet.com
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

// hue output

#include "squeezelite.h"
#include "libhuec.h"

extern log_level	output_loglevel;
static log_level 	*loglevel = &output_loglevel;

#define LOCK   mutex_lock(ctx->outputbuf->mutex)
#define UNLOCK mutex_unlock(ctx->outputbuf->mutex)
#define LOCK_S   mutex_lock(ctx->streambuf->mutex)
#define UNLOCK_S mutex_unlock(ctx->streambuf->mutex)

__u8 *disco_buf;
int disco_frames;

/*---------------------------------------------------------------------------*/
void wake_output(struct thread_ctx_s *ctx) {
    return;
}


/*---------------------------------------------------------------------------*/
static int _hue_write_frames(struct thread_ctx_s *ctx, frames_t out_frames, bool silence, s32_t gainL, s32_t gainR,
                                s32_t cross_gain_in, s32_t cross_gain_out, s16_t **cross_ptr) {

    s16_t *obuf;

    if (!silence) {

        if (ctx->output.fade == FADE_ACTIVE && ctx->output.fade_dir == FADE_CROSS && *cross_ptr) {
            _apply_cross(ctx->outputbuf, out_frames, cross_gain_in, cross_gain_out, cross_ptr);
        }

        obuf = (s16_t*) ctx->outputbuf->readp;

    }
    else {

        obuf = (s16_t*) ctx->silencebuf;
    }

    _scale_frames((s16_t*) (ctx->output.buf + ctx->output.buf_frames * BYTES_PER_FRAME), obuf, out_frames, gainL, gainR);

    ctx->output.buf_frames += out_frames;

    return (int) out_frames;
}


/*---------------------------------------------------------------------------*/
void output_close(struct thread_ctx_s *ctx)
{
    output_close_common(ctx);
    free(ctx->output.buf);
    free(disco_buf);
}

/*---------------------------------------------------------------------------*/
int disco_send_chunk(void *device, __u8 *frame_buf, int number_frames){
    LOG_SDEBUG("[%p]: analyzing chunk with %d frames for disco", device, number_frames);

    hue_bridge_t *bridge;
    bridge = device;

    float sum = 0.0;
    float average_sum = 0.0;

    for (int i = 0; i < number_frames; i++){
        sum += pow( (float)frame_buf[i] / pow(2, (sizeof(frame_buf[i])*8 - 1)), 2);
    }

    average_sum = sum / number_frames;

    LOG_SDEBUG("[%p]: got volume of %f", device, average_sum);

    if (average_sum > 0.4) {
        LOG_INFO("HIT");

        hue_light_t hueLight;

            hueLight.attribute.id = 2;
            strcpy(hueLight.attribute.name, "Dieter");

            hue_set_light_state(bridge, &hueLight, SWITCH, "ON");

    }
    else {
        LOG_INFO("NO HIT.");

        hue_light_t hueLight;

            hueLight.attribute.id = 2;
            strcpy(hueLight.attribute.name, "Dieter");

            hue_set_light_state(bridge, &hueLight, SWITCH, "OFF");

    }

    return 0;
}

/*---------------------------------------------------------------------------*/
static void *output_hue_thread(struct thread_ctx_s *ctx) {
    while (ctx->output_running) {
        bool ran = false;

        // proceed only if room in queue *and* running
        if (ctx->output.state >= OUTPUT_BUFFER) {

            LOCK;
            // this will internally loop till we have exactly 352 frames
            _output_frames(FRAMES_PER_BLOCK, ctx);
            UNLOCK;

            // nothing to do, sleep
            if (ctx->output.buf_frames) {
                usleep(FRAMES_PER_BLOCK * 1000000/ 44100);

                memcpy((disco_buf + disco_frames * BYTES_PER_FRAME), ctx->output.buf, (ctx->output.buf_frames * BYTES_PER_FRAME));
                disco_frames += ctx->output.buf_frames;

                if( (FRAMES_PER_BLOCK * 16) == disco_frames ){
                    LOG_SDEBUG("[%p]: sending chunk to %s", ctx->output.device, ((hue_bridge_t *)ctx->output.device)->name);
                    disco_send_chunk(ctx->output.device, disco_buf, disco_frames);
                    disco_frames = 0;
                }

                // current block is a track start, set the value
                if (ctx->output.detect_start_time) {
                    ctx->output.detect_start_time = false;
                    ctx->output.track_start_time = gettime_ms();
                    LOG_INFO("[%p]: track actual start time:%u (gap:%d)", ctx, ctx->output.track_start_time,
                                    (s32_t) (ctx->output.track_start_time - ctx->output.start_at));
                }

                ctx->output.buf_frames = 0;
                ran = true;
            }
        }

        LOCK;
        ctx->output.updated = gettime_ms();
        // check that later: shall we change queued_frames when not running?
        // or a keep-alive when no frame sent
        ctx->output.device_frames = 0;
        ctx->output.device_true_frames = 0;
        ctx->output.frames_played_dmp = ctx->output.frames_played;
        UNLOCK;

        if (!ran) usleep(10000);
    }

    return 0;
}


/*---------------------------------------------------------------------------*/
void output_hue_thread_init(hue_bridge_t *hue, unsigned output_buf_size, struct thread_ctx_s *ctx) {
    pthread_attr_t attr;

	LOG_INFO("[%p]: init output hue", ctx);

    memset(&ctx->output, 0, sizeof(ctx->output));

    ctx->output.buf = malloc(FRAMES_PER_BLOCK * BYTES_PER_FRAME);
    if (!ctx->output.buf) {
        LOG_ERROR("[%p]: unable to malloc buf", ctx);
        return;
    }

    disco_buf = malloc(16 * FRAMES_PER_BLOCK * BYTES_PER_FRAME);
    disco_frames = 0;
    
    ctx->output_running = true;
    ctx->output.buf_frames = 0;
    ctx->output.start_frames = FRAMES_PER_BLOCK * 2;
    ctx->output.write_cb = &_hue_write_frames;

    output_init_common(hue, output_buf_size, 44100, ctx);

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN + OUTPUT_THREAD_STACK_SIZE);
    pthread_create(&ctx->output_thread, &attr, (void *(*)(void*)) &output_hue_thread, ctx);
    pthread_attr_destroy(&attr);
}




