#ifndef __HUE_AUDIO_H_
#define __HUE_AUDIO_H_

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "platform.h"
#include "fftw3.h"

struct huebridgecl_s;

typedef enum {
    MONO,
    STEREO
} channels_t; 

typedef enum {
    AVERAGE,
    LEFT,
    RIGHT
} mono_option_t;

typedef struct hueaudio_s {
    channels_t channels;
    mono_option_t mono_option;
    int bass_cut_off;
    int treble_cut_off;
    int lower_cut_off;
    int upper_cut_off;

    int height;
    int sampling_rate;

    int sense;
    double ignore;

    bool autosense;
    bool senselow;

    int waves;
    double monstercat;

    double gravity;
    double integral;

    int number_of_bars;

    int *brightness;

    bool first;

    int FFTbassbufferSize;
    int FFTmidbufferSize;
    int FFTtreblebufferSize;
    double *bass_multiplier;
    double *mid_multiplier;
    double *treble_multiplier;
    double *in_bass_r_raw, *in_bass_l_raw;
    double *in_mid_r_raw, *in_mid_l_raw;
    double *in_treble_r_raw, *in_treble_l_raw;
    double *in_bass_r, *in_bass_l;
    double *in_mid_r, *in_mid_l;
    double *in_treble_r, *in_treble_l;
    fftw_complex *out_bass_l, *out_bass_r;
    fftw_plan p_bass_l, p_bass_r;
    fftw_complex *out_mid_l, *out_mid_r;
    fftw_plan p_mid_l, p_mid_r;
    fftw_complex *out_treble_l, *out_treble_r;
    fftw_plan p_treble_l, p_treble_r;
} hueaudio_data_t;

struct hueaudio_s *hue_audio_create(void);
bool hue_audio_destroy(struct hueaudio_s *p);

void hue_set_fft_buffers_to_zero(struct hueaudio_s *p);
bool hue_write_to_fft_input_buffers(s16_t frames, s16_t buf[frames * 2], struct hueaudio_s *p);
bool hue_analyze_audio(struct hueaudio_s *p);

#endif /* __HUE_AUDIO_H_ */
