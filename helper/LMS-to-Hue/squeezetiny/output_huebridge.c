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


#include "squeezelite.h"
#include "hue_bridge.h"

#define LOCK   mutex_lock(ctx->outputbuf->mutex)
#define UNLOCK mutex_unlock(ctx->outputbuf->mutex)
#define LOCK_S   mutex_lock(ctx->streambuf->mutex)
#define UNLOCK_S mutex_unlock(ctx->streambuf->mutex)

extern log_level    huebridge_loglevel;
static log_level    *loglevel = &huebridge_loglevel;

/*---------------------------------------------------------------------------*/
void wake_output(struct thread_ctx_s *ctx) {
    return;
}


/*---------------------------------------------------------------------------*/
static int _huebridge_write_frames(struct thread_ctx_s *ctx, frames_t out_frames, bool silence, s32_t gainL, s32_t gainR,
                             u8_t flags, s32_t cross_gain_in, s32_t cross_gain_out, s32_t **cross_ptr) {

    u8_t *obuf;

    if (!silence) {
        if (ctx->output.fade == FADE_ACTIVE && ctx->output.fade_dir == FADE_CROSS && *cross_ptr) {
            _apply_cross(ctx->outputbuf, out_frames, cross_gain_in, cross_gain_out, cross_ptr);
        }

        obuf = ctx->outputbuf->readp;
    }
    else {
        obuf = ctx->silencebuf;
    }

    _scale_and_pack_frames((ctx->output.buf + ctx->output.buf_frames * BYTES_PER_FRAME), (s32_t*)(void *)obuf, out_frames, gainL, gainR, flags, ctx->output.format);

    ctx->output.buf_frames += out_frames;

    return (int) out_frames;
}


/*---------------------------------------------------------------------------*/
void output_close(struct thread_ctx_s *ctx) {
    output_close_common(ctx);
    free(ctx->output.buf);
}

/*---------------------------------------------------------------------------*/
static void *output_huebridge_thread(struct thread_ctx_s *ctx) {

    while (ctx->output_running) {
        bool ran = false;

        // proceed only if room in queue *and* running
        if (ctx->output.state >= OUTPUT_BUFFER && huebridge_accept_frames(ctx->output.device)) {
            u64_t playtime;

            LOCK;
            // this will internally loop till we have exactly 4096 frames
            _output_frames(FRAMES_PER_BLOCK, ctx);
            UNLOCK;

            if (ctx->output.buf_frames) {
                huebridge_process_chunk(ctx->output.device, ctx->output.buf, ctx->output.buf_frames, &playtime);

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
        ctx->output.frames_played_dmp = ctx->output.frames_played;
        UNLOCK;

        if (!ran)
            usleep(10000);
    }

    return 0;
}


/*---------------------------------------------------------------------------*/
bool output_huebridge_thread_init(struct huebridgecl_s *huebridgecl, unsigned outputbuf_size, struct thread_ctx_s *ctx) {
    pthread_attr_t attr;

    LOG_INFO("[%p]: init output hue", ctx);

    memset(&ctx->output, 0, sizeof(ctx->output));

    ctx->output.buf = malloc(FRAMES_PER_BLOCK * BYTES_PER_FRAME);
    if (!ctx->output.buf) {
        LOG_ERROR("[%p]: unable to malloc buf", ctx);

        return false;
    }

    ctx->output_running = true;
    ctx->output.format = S16_LE;
    ctx->output.buf_frames = 0;
    ctx->output.start_frames = FRAMES_PER_BLOCK * 2;
    ctx->output.write_cb = &_huebridge_write_frames;

    output_init_common(huebridgecl, outputbuf_size, 44100, ctx);

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN + OUTPUT_THREAD_STACK_SIZE);
    pthread_create(&ctx->output_thread, &attr, (void *(*)(void*)) &output_huebridge_thread, ctx);
    pthread_attr_destroy(&attr);

    return true;
}
