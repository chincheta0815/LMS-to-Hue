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
#include "virtual.h"

#include "libhuec.h"
#include "aubio.h"
#include "biquad.h"

extern log_level	output_loglevel;
static log_level 	*loglevel = &output_loglevel;

#define LOCK   mutex_lock(ctx->outputbuf->mutex)
#define UNLOCK mutex_unlock(ctx->outputbuf->mutex)
#define LOCK_S   mutex_lock(ctx->streambuf->mutex)
#define UNLOCK_S mutex_unlock(ctx->streambuf->mutex)

aubio_tempo_t *aubio_tempo;
fvec_t *aubio_tempo_in;
fvec_t *aubio_tempo_out;

biquad *bandpass;
biquad *lowpass;
smp_type old_env;

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
void output_close(struct thread_ctx_s *ctx) {
    output_close_common(ctx);
    free(ctx->output.buf);
    /*
    del_aubio_tempo(aubio_tempo);
    del_fvec(aubio_tempo_in);
    del_fvec(aubio_tempo_out);
     */
     free(bandpass);
}


/*---------------------------------------------------------------------------*/
int disco_process_chunk(void *device, __u8 *frame_buf, int num_frames){
    LOG_INFO("[%p]: analyzing chunk with %d frames for disco", device, num_frames);

    hue_bridge_t *bridge;
    bridge = device;
    
    /*
    for (int i = 0; i < num_frames; i++) {
        aubio_tempo_in->data[i] =  (smpl_t)frame_buf[i];
    }

    aubio_tempo_do(aubio_tempo, aubio_tempo_in, aubio_tempo_out);
     */
 
    smp_type sample[num_frames];
    for (int i = 0; i < num_frames; i++) {
        sample[i] = BiQuad(((smp_type)frame_buf[i]-128)/128, lowpass);
        if (sample[i] < 0) sample[i] = -sample[i];
        if (sample[i] <= old_env) sample[i] = old_env * 0.99;
        old_env = sample[i];
    }
 
    LOG_INFO("Value: %f", sample[num_frames-1]);
    if(sample[num_frames-1] > 0.2) {
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


/*---------------------------------------------------------------------------*/
static void *output_hue_thread(struct thread_ctx_s *ctx) {
    while (ctx->output_running) {
        bool ran = false;

        // proceed only if room in queue *and* running
        if (ctx->output.state >= OUTPUT_BUFFER && virtual_accept_frames(ctx->output.vplayer_device)) {
            u64_t playtime;

            LOCK;
            // this will internally loop till we have exactly 352 frames
            _output_frames(FRAMES_PER_BLOCK, ctx);
            UNLOCK;

            // nothing to do, sleep
            if (ctx->output.buf_frames) {
                LOG_INFO("[%p]: sending chunk to %s", ctx->output.vplayer_device, ((hue_bridge_t *)ctx->output.light_device)->name);
                // Start virtual player for simple time sample consumption (real time).
                virtual_send_chunk(ctx->output.vplayer_device, ctx->output.buf, ctx->output.buf_frames, &playtime);

                // Invoke sample processing and light setting (cpu time).
                disco_process_chunk(ctx->output.light_device, ctx->output.buf, ctx->output.buf_frames);

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
void output_hue_thread_init(void *vplayer, hue_bridge_t *hue, unsigned output_buf_size, struct thread_ctx_s *ctx) {
    pthread_attr_t attr;

    LOG_INFO("[%p]: init output hue", ctx);

    memset(&ctx->output, 0, sizeof(ctx->output));

    ctx->output.buf = malloc(FRAMES_PER_BLOCK * BYTES_PER_FRAME);
    if (!ctx->output.buf) {
        LOG_ERROR("[%p]: unable to malloc buf", ctx);

        return;
    }

    /*
    int buf_size = 1024;
    int hop_size = buf_size / 2;
    aubio_tempo = new_aubio_tempo("default", buf_size, hop_size, 44100);
    aubio_tempo_in = new_fvec(buf_size);
    aubio_tempo_out = new_fvec(1);
     */

    smp_type freq_min = 20;
    smp_type freq_max = 200;
    smp_type cf = sqrt(freq_max * freq_min);
    smp_type bw = 1 / log10(2) * log10(freq_max / freq_min);
    bandpass = BiQuad_new(BPF, 0, cf, 44100, bw);
    
    lowpass = BiQuad_new(LPF, 0, 100, 44100 , 1.916853);
    
    old_env = 0;

    ctx->output_running = true;
    ctx->output.buf_frames = 0;
    ctx->output.start_frames = FRAMES_PER_BLOCK * 2;
    ctx->output.write_cb = &_hue_write_frames;

    output_init_common(vplayer, hue, output_buf_size, 44100, ctx);

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN + OUTPUT_THREAD_STACK_SIZE);
    pthread_create(&ctx->output_thread, &attr, (void *(*)(void*)) &output_hue_thread, ctx);
    pthread_attr_destroy(&attr);
}
