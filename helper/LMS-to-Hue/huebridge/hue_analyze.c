#include "hue_analyze.h"
#include "hue_bridge.h"
#include "hue_entertainment.h"

#include "log_util.h"

extern log_level    huebridge_loglevel;
static log_level    *loglevel = &huebridge_loglevel;

static double _smoothDef[64] = {
                                0.8, 0.8, 1, 1, 0.8, 0.8, 1, 0.8, 0.8, 1, 1, 0.8,
                                1, 1, 0.8, 0.6, 0.6, 0.7, 0.8, 0.8, 0.8, 0.8, 0.8,
                                0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8,
                                0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8,
                                0.7, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6,
                                0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6
                               };


static int *separate_freq_bands(fftw_complex out[MAX_SAMPLES_PER_CHUNK / 2 + 1], int bars, int lcf[200], int hcf[200],
                                float k[200], int channel, double sense, double ignore) {
    int i, j;
    float peak[200];
    static int fl[200];
    static int fr[200];
    int y[MAX_SAMPLES_PER_CHUNK / 2 + 1];
    float temp;

    // separate frequency bands
    for (i = 0; i < bars; i++) {
        peak[i] = 0;

        // get peaks
        for (j = lcf[i]; j <= hcf[i]; j++) {
            // getting r of complex
            y[j] = hypot(out[j][0], out[j][1]);
            peak[i] += y[j];
        }

        peak[i] = peak[i] / (hcf[i] - lcf[i] + 1);
        temp = peak[i] + k[i] * sense;

        if (temp <= ignore)
            temp = 0;

        if (channel == 1)
            fl[i] = temp;
        else
            fr[i] = temp;
    }

    if (channel == 1)
        return fl;
    else
        return fr;
}

static bool hue_analyze_prepare_light_signal(struct huebridgecl_s *p) {
    int i;
    u16_t value;
    int comp;
    int red;
    int green;
    int blue;

    comp = 65534;

    if (p->flash_mode == HUE_LIGHT_FREQ) {
        for (i = 0; i < p->hue_ent_ctx.light_count; i++) {
            value = (p->hue_analyze_ctx.f[i] <  comp ? p->hue_analyze_ctx.f[i] : comp);
            LOG_SDEBUG("[%p]: flash mode %d, sending value %d", p, p->flash_mode, value);
            hue_ent_set_light(&p->hue_ent_ctx, i, value, value, value);
        }
    }
    else if (p->flash_mode == HUE_COLOR_FREQ) {
        for (i = 0; i < p->hue_ent_ctx.light_count; i++) {
            red = (p->hue_analyze_ctx.f[0] < comp ? p->hue_analyze_ctx.f[0] : comp);
            green = (p->hue_analyze_ctx.f[1] < comp ? p->hue_analyze_ctx.f[1] : comp);
            blue = (p->hue_analyze_ctx.f[2] < comp ? p->hue_analyze_ctx.f[2] : comp);
            LOG_SDEBUG("[%p]: flash mode %d, sending values red %d, green %d, blue %d", p, p->flash_mode, red, green, blue);
            hue_ent_set_light(&p->hue_ent_ctx, i, red, green, blue);
        }
    }

    return true;
}

bool hue_analyze_audio(struct huebridgecl_s *p, s16_t channel_left[MAX_SAMPLES_PER_CHUNK], s16_t channel_right[MAX_SAMPLES_PER_CHUNK]) {
    int i;
    double freqconst;
    double audiorate = 44100;
    double gravity = 100;
    int framerate = 50;
    int smcount = 64;
    unsigned lowcf = 50;
    unsigned highcf = 10000;
    int autosense = 1;
    double integral = 90 / 100;
    int diff;
    double div;

    p->hue_analyze_ctx.sense = 1.0;
    p->hue_analyze_ctx.senselow = true;
    p->hue_analyze_ctx.pl = fftw_plan_dft_r2c_1d(MAX_SAMPLES_PER_CHUNK, p->hue_analyze_ctx.inl, p->hue_analyze_ctx.outl, FFTW_MEASURE);

    if (p->flash_mode == HUE_LIGHT_FREQ) {
        p->hue_analyze_ctx.bars = p->hue_ent_ctx.light_count;
    }
    else if (p->flash_mode == HUE_COLOR_FREQ) {
        p->hue_analyze_ctx.bars = 3;
    }

    p->hue_analyze_ctx.g = gravity * ((float)p->hue_analyze_ctx.height / 2160.) * pow(60. / (float)framerate, 2.5);

    if ((smcount > 0) && (p->hue_analyze_ctx.bars))
        p->hue_analyze_ctx.smh = (double)(((double)smcount) / ((double)p->hue_analyze_ctx.bars));

    freqconst = log10((float)lowcf / (float)highcf)
                / (1. / (((float)p->hue_analyze_ctx.bars) + 1.) - 1.);

    // calculate cutoff frequencies
    for (i = 0; i < p->hue_analyze_ctx.bars; i++) {
        p->hue_analyze_ctx.fc[i] = highcf * pow(10, -1. * freqconst + ((((float)i + 1.) / ((float)p->hue_analyze_ctx.bars + 1.)) * freqconst));
        p->hue_analyze_ctx.fre[i] = p->hue_analyze_ctx.fc[i] / (audiorate / 5);

        // lcf stores the lower cut frequency for each bar in the fft out buffer
        p->hue_analyze_ctx.lcf[i] = p->hue_analyze_ctx.fre[i] * (MAX_SAMPLES_PER_CHUNK / 2.);

        if (i != 0) {
            p->hue_analyze_ctx.hcf[i-1] = p->hue_analyze_ctx.lcf[i] - 1.;

            // pushing the spectrum up if the expe function gets "clumped"
            if (p->hue_analyze_ctx.lcf[i] <= p->hue_analyze_ctx.lcf[i-1]) 
                p->hue_analyze_ctx.lcf[i] = p->hue_analyze_ctx.lcf[i-1] + 1.;

            p->hue_analyze_ctx.hcf[i-1] = p->hue_analyze_ctx.lcf[i] - 1.;
        }
    }

    // weigh signals to frequencies
    for (i = 0; i < p->hue_analyze_ctx.bars; i++) {
        p->hue_analyze_ctx.k[i] = pow(p->hue_analyze_ctx.fc[i], 0.85) * ((float)p->hue_analyze_ctx.height / (MAX_SAMPLES_PER_CHUNK* 32000))
                                  * _smoothDef[(int)floor(((double)i) * p->hue_analyze_ctx.smh)];
    }

    // populate input buffer and check if input is present
    p->hue_analyze_ctx.silence = 1;
    for (i = 0; i < (2 * (MAX_SAMPLES_PER_CHUNK / 2 + 1)); i++) {
        if (i < MAX_SAMPLES_PER_CHUNK) {
            p->hue_analyze_ctx.inl[i] = channel_left[i];
            if (p->hue_analyze_ctx.inl[i])
                p->hue_analyze_ctx.silence = 0;
        }
        else {
            p->hue_analyze_ctx.inl[i] = 0;
        }
    }

    if (p->hue_analyze_ctx.silence == 1)
        p->hue_analyze_ctx.sleep++;
    else
        p->hue_analyze_ctx.sleep = 0;

    //if input was present for the last 5 seconds apply FFT to it
    if (p->hue_analyze_ctx.sleep < audiorate * 1) {

        //execute FFT and sort frequency bands
       fftw_execute(p->hue_analyze_ctx.pl);
       p->hue_analyze_ctx.fl = separate_freq_bands(p->hue_analyze_ctx.outl, p->hue_analyze_ctx.bars, p->hue_analyze_ctx.lcf, p->hue_analyze_ctx.hcf,
                                                   p->hue_analyze_ctx.k, 1, p->hue_analyze_ctx.sense, 0); 
    }
    else {
        return false;
    }

    // preparing signal for drawing
    for (i = 0; i < p->hue_analyze_ctx.bars; i++)
        p->hue_analyze_ctx.f[i] = p->hue_analyze_ctx.fl[i];

    // smooting falloff
    if (p->hue_analyze_ctx.g > 0) {
        for (i = 0; i < p->hue_analyze_ctx.bars; i++) {
            if (p->hue_analyze_ctx.f[i] < p->hue_analyze_ctx.flast[i]) {
                p->hue_analyze_ctx.f[i] = p->hue_analyze_ctx.fpeak[i] - (p->hue_analyze_ctx.g * pow(p->hue_analyze_ctx.fall[i], 2));
                p->hue_analyze_ctx.fall[i]++;
            }
            else {
                p->hue_analyze_ctx.fpeak[i] = p->hue_analyze_ctx.f[i];
                p->hue_analyze_ctx.fall[i] = 0;
            }
            p->hue_analyze_ctx.flast[i] = p->hue_analyze_ctx.f[i];
        }
    }

    // smoothing integral
    if (integral > 0) {
        for (i = 0; i < p->hue_analyze_ctx.bars; i++) {
            p->hue_analyze_ctx.f[i] = p->hue_analyze_ctx.fmem[i] * integral + p->hue_analyze_ctx.f[i];
            p->hue_analyze_ctx.fmem[i] = p->hue_analyze_ctx.f[i];

            diff = (p->hue_analyze_ctx.height + 1) - p->hue_analyze_ctx.f[i];
            if (diff < 0)
                diff = 0;
            div = 1. / (diff + 1.);

            p->hue_analyze_ctx.fmem[i] = p->hue_analyze_ctx.fmem[i] * (1. - div / 20.);
        }
    }

    // zero values cause divided by zero segfaults
    for (i = 0; i < p->hue_analyze_ctx.bars; i++) {
        if (p->hue_analyze_ctx.f[i] < 1)
            p->hue_analyze_ctx.f[i] = 0;
    }

    // automatic sense adjustment
    if (autosense) {
        for (i = 0; i < p->hue_analyze_ctx.bars; i++) {
            if (p->hue_analyze_ctx.f[i] > p->hue_analyze_ctx.height) {
                p->hue_analyze_ctx.senselow = false;
                p->hue_analyze_ctx.sense *= 0.985;
                break;
            }

            if (p->hue_analyze_ctx.senselow && !p->hue_analyze_ctx.silence)
                p->hue_analyze_ctx.sense *= 1.01;

            if (i == p->hue_analyze_ctx.bars - 1)
                p->hue_analyze_ctx.sense *= 1.002; 
        }
    }

    hue_analyze_prepare_light_signal(p);

    return true;
}
