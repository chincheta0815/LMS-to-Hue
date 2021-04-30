#ifndef __HUE_ANALYZE_H_
#define __HUE_ANALYZE_H_

#include "platform.h"
#include "fftw3.h"

#ifndef MAX_SAMPLES_PER_CHUNK
#define MAX_SAMPLES_PER_CHUNK 4096
#endif

typedef enum {
                MONO   = 1,
                STEREO = 2
} audio_type_t;

typedef enum {
        HUE_LIGHT_FREQ,
        HUE_COLOR_FREQ
} flash_mode_t;

struct huebridgecl_s;

struct hue_analyze_ctx {
        int bars;
        double g;
        int height;
        int silence;
        int sleep;
        flash_mode_t mode;
        unsigned f[200];
        int *fl;
        int flast[200];
        int fall[200];
        int fpeak[200];
        float k[200];
        float fc[200];
        float fre[200];
        int lcf[200];
        int hcf[200];
        double smh;
        double inl[2 * (MAX_SAMPLES_PER_CHUNK / 2 + 1)];
        fftw_complex outl[2 * (MAX_SAMPLES_PER_CHUNK / 2 + 1)];
        double sense;
        bool senselow;
        fftw_plan pl;
        int fmem[200];
};

bool hue_analyze_audio(struct huebridgecl_s *p, s16_t channel_left[MAX_SAMPLES_PER_CHUNK/2], s16_t channel_right[MAX_SAMPLES_PER_CHUNK/2]);

#endif /* __HUE_ANALYZE_H_ */
