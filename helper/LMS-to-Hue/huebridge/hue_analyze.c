#include "hue_analyze.h"
#include "hue_bridge.h"

#include "log_util.h"

#define REAL 0
#define IMG 1

extern log_level    huebridge_loglevel;
static log_level    *loglevel = &huebridge_loglevel;

struct hueaudio_s *hue_audio_create(void) {
    struct hueaudio_s *p;
    p = malloc(sizeof(struct hueaudio_s));
    memset(p, 0, sizeof(struct hueaudio_s));

    LOG_SDEBUG("[%p]: creating hue audio context", p);

    p->channels = MONO;
    p->mono_option = AVERAGE;

    p->bass_cut_off = 150;
    p->treble_cut_off = 2500;

    p->lower_cut_off = 50;
    p->upper_cut_off = 10000;

    p->height = 65534;
    p->sampling_rate = 44100;
    p->ignore = 0;
    p->sense = 100;

    p->autosense = true;
    p->senselow = true;

    p->monstercat = 0;
    p->monstercat *= 1.5;

    p->waves = 0;

    p->gravity = 100;
    p->gravity = p->gravity / 100;

    p->integral = 77;
    p->integral = p->integral / 100;
    
    p->number_of_bars = 256;

    p->first = true;

    p->FFTbassbufferSize = 4096;
    p->FFTmidbufferSize = 2048;
    p->FFTtreblebufferSize = 1024;

    // BASS
    p->in_bass_l_raw = fftw_alloc_real(p->FFTbassbufferSize);
    p->in_bass_r_raw = fftw_alloc_real(p->FFTbassbufferSize);
    p->in_bass_l = fftw_alloc_real(p->FFTbassbufferSize);
    p->in_bass_r = fftw_alloc_real(p->FFTbassbufferSize);

    p->out_bass_l = fftw_alloc_complex(p->FFTbassbufferSize / 2 + 1);
    p->out_bass_r = fftw_alloc_complex(p->FFTbassbufferSize / 2 + 1);
    memset(p->out_bass_l, 0, (p->FFTbassbufferSize / 2 + 1) * sizeof(fftw_complex));
    memset(p->out_bass_r, 0, (p->FFTbassbufferSize / 2 + 1) * sizeof(fftw_complex));

    p->p_bass_l = fftw_plan_dft_r2c_1d(p->FFTbassbufferSize, p->in_bass_l, p->out_bass_l, FFTW_MEASURE);
    p->p_bass_r = fftw_plan_dft_r2c_1d(p->FFTbassbufferSize, p->in_bass_r, p->out_bass_r, FFTW_MEASURE);

    p->bass_multiplier = malloc(p->FFTbassbufferSize * sizeof(double));
    for (int i = 0; i < p->FFTbassbufferSize; i++) {
        p->bass_multiplier[i] = 0.5 * (1 - cos(2 * M_PI * i / (p->FFTbassbufferSize -1)));
    }

    // MID
    p->in_mid_l_raw = fftw_alloc_real(p->FFTmidbufferSize);
    p->in_mid_r_raw = fftw_alloc_real(p->FFTmidbufferSize);
    p->in_mid_l = fftw_alloc_real(p->FFTmidbufferSize);
    p->in_mid_r = fftw_alloc_real(p->FFTmidbufferSize);

    p->out_mid_l = fftw_alloc_complex(p->FFTmidbufferSize / 2 + 1);
    p->out_mid_r = fftw_alloc_complex(p->FFTmidbufferSize / 2 + 1);
    memset(p->out_mid_l, 0, (p->FFTmidbufferSize / 2 + 1) * sizeof(fftw_complex));
    memset(p->out_mid_r, 0, (p->FFTmidbufferSize / 2 + 1) * sizeof(fftw_complex));

    p->p_mid_l = fftw_plan_dft_r2c_1d(p->FFTmidbufferSize, p->in_mid_l, p->out_mid_l, FFTW_MEASURE);
    p->p_mid_r = fftw_plan_dft_r2c_1d(p->FFTmidbufferSize, p->in_mid_r, p->out_mid_r, FFTW_MEASURE);

    p->mid_multiplier = malloc(p->FFTmidbufferSize * sizeof(double));
    for (int i = 0; i < p->FFTmidbufferSize; i++) {
        p->mid_multiplier[i] = 0.5 * (1  - cos(2 * M_PI * i / (p->FFTmidbufferSize -1))); 
    }

    // TREBLE
    p->in_treble_l_raw = fftw_alloc_real(p->FFTtreblebufferSize);
    p->in_treble_r_raw = fftw_alloc_real(p->FFTtreblebufferSize);
    p->in_treble_l = fftw_alloc_real(p->FFTtreblebufferSize);
    p->in_treble_r = fftw_alloc_real(p->FFTtreblebufferSize);

    p->out_treble_l = fftw_alloc_complex(p->FFTtreblebufferSize / 2 + 1);
    p->out_treble_r = fftw_alloc_complex(p->FFTtreblebufferSize / 2 + 1);
    memset(p->out_treble_l, 0, (p->FFTtreblebufferSize / 2 + 1) * sizeof(fftw_complex));
    memset(p->out_treble_r, 0, (p->FFTtreblebufferSize / 2 + 1) * sizeof(fftw_complex));

    p->p_treble_l = fftw_plan_dft_r2c_1d(p->FFTtreblebufferSize, p->in_treble_l, p->out_treble_l, FFTW_MEASURE);
    p->p_treble_r = fftw_plan_dft_r2c_1d(p->FFTtreblebufferSize, p->in_treble_r, p->out_treble_r, FFTW_MEASURE);

    p->treble_multiplier = malloc(p->FFTtreblebufferSize * sizeof(double));
    for (int i = 0; i < p->FFTtreblebufferSize; i++) {
        p->treble_multiplier[i] = 0.5 * (1 - cos(2 * M_PI * i / (p->FFTtreblebufferSize - 1)));
    }

    p->brightness = malloc(p->number_of_bars * sizeof(int));

    // fill buffer with zero for init
    hue_set_fft_buffers_to_zero(p);

    return p;
}

bool hue_audio_destroy(struct hueaudio_s *p) {
    if (!p)
        return false;

    fftw_free(p->in_bass_r);
    fftw_free(p->in_bass_l);
    fftw_free(p->out_bass_r);
    fftw_free(p->out_bass_l);
    fftw_destroy_plan(p->p_bass_l);
    fftw_destroy_plan(p->p_bass_r);

    fftw_free(p->in_mid_r);
    fftw_free(p->in_mid_l);
    fftw_free(p->out_mid_r);
    fftw_free(p->out_mid_l);
    fftw_destroy_plan(p->p_mid_l);
    fftw_destroy_plan(p->p_mid_r);

    fftw_free(p->in_treble_r);
    fftw_free(p->in_treble_l);
    fftw_free(p->out_treble_r);
    fftw_free(p->out_treble_l);
    fftw_destroy_plan(p->p_treble_l);
    fftw_destroy_plan(p->p_treble_r);

    free(p);

    return true;
}

bool hue_write_to_fft_input_buffers(s16_t frames, s16_t *buf, struct hueaudio_s *p) {
    if (frames == 0)
        return false;

    for (u16_t n = p->FFTbassbufferSize; n > frames; n = n - frames) {
        for (u16_t i = 1; i <= frames; i++) {
            p->in_bass_l_raw[n - i] = p->in_bass_l_raw[n - i - frames];
            if (p->channels == STEREO)
                p->in_bass_r_raw[n - i] = p->in_bass_r_raw[n - i - frames];
        }
    }
    for (u16_t n = p->FFTmidbufferSize; n > frames; n = n - frames) {
        for (u16_t i = 1; i <= frames; i++) {
            p->in_mid_l_raw[n - i] = p->in_mid_l_raw[n - i - frames];
            if (p->channels == STEREO)
                p->in_mid_r_raw[n - i] = p->in_mid_r_raw[n - i - frames];
        }
    }
    for (u16_t n = p->FFTtreblebufferSize; n > frames; n = n - frames) {
        for (u16_t i = 1; i <= frames; i++) {
            p->in_treble_l_raw[n - i] = p->in_treble_l_raw[n - i - frames];
            if (p->channels == STEREO)
                p->in_treble_r_raw[n - i] = p->in_treble_r_raw[n - i - frames];
        }
    }
    u16_t n = frames - 1;
    for (u16_t i = 0; i < frames; i++) {
        if (p->channels == MONO) {
            if (p->mono_option == AVERAGE) {
                p->in_bass_l_raw[n] = (buf[i * 2] + buf[i * 2 + 1]) / 2;
            }
            if (p->mono_option == LEFT) {
                p->in_bass_l_raw[n] = buf[i * 2];
            }
            if (p->mono_option == RIGHT) {
                p->in_bass_l_raw[n] = buf[i * 2 + 1];
            }
        }
        // stereo storing channels in buffer
        if (p->channels == STEREO) {
            p->in_bass_l_raw[n] = buf[i * 2];
            p->in_bass_r_raw[n] = buf[i * 2 + 1];

            p->in_mid_r_raw[n] = p->in_bass_r_raw[n];
            p->in_treble_r_raw[n] = p->in_bass_r_raw[n];
        }

        p->in_mid_l_raw[n] = p->in_bass_l_raw[n];
        p->in_treble_l_raw[n] = p->in_bass_l_raw[n];
        n--;
    }

    // Hann Window
    for (int i = 0; i < p->FFTbassbufferSize; i++) {
        p->in_bass_l[i] = p->bass_multiplier[i] * p->in_bass_l_raw[i];
        if (p->channels == STEREO)
            p->in_bass_r[i] = p->bass_multiplier[i] * p->in_bass_r_raw[i];
    }
    for (int i = 0; i < p->FFTmidbufferSize; i++) {
        p->in_mid_l[i] = p->mid_multiplier[i] * p->in_mid_l_raw[i];
        if (p->channels == STEREO)
            p->in_mid_r[i] = p->mid_multiplier[i] * p->in_mid_r_raw[i];
    }
    for (int i = 0; i < p->FFTtreblebufferSize; i++) {
        p->in_treble_l[i] = p->treble_multiplier[i] * p->in_treble_l_raw[i];
        if (p->channels == STEREO)
            p->in_treble_r[i] = p->treble_multiplier[i] * p->in_treble_r_raw[i];
    }

    return true;
}

void hue_set_fft_buffers_to_zero(struct hueaudio_s *p) {
    memset(p->in_bass_r, 0, sizeof(double) * p->FFTbassbufferSize);
    memset(p->in_bass_l, 0, sizeof(double) * p->FFTbassbufferSize);
    memset(p->in_mid_r, 0, sizeof(double) * p->FFTmidbufferSize);
    memset(p->in_mid_l, 0, sizeof(double) * p->FFTmidbufferSize);
    memset(p->in_treble_r, 0, sizeof(double) * p->FFTtreblebufferSize);
    memset(p->in_treble_l, 0, sizeof(double) * p->FFTtreblebufferSize);
    memset(p->in_bass_r_raw, 0, sizeof(double) * p->FFTbassbufferSize);
    memset(p->in_bass_l_raw, 0, sizeof(double) * p->FFTbassbufferSize);
    memset(p->in_mid_r_raw, 0, sizeof(double) * p->FFTmidbufferSize);
    memset(p->in_mid_l_raw, 0, sizeof(double) * p->FFTmidbufferSize);
    memset(p->in_treble_r_raw, 0, sizeof(double) * p->FFTtreblebufferSize);
    memset(p->in_treble_l_raw, 0, sizeof(double) * p->FFTtreblebufferSize);
}

static bool _calculate_cutoff_and_eq(struct hueaudio_s *p, int *out_bass_cut_off_bar, int *out_treble_cut_off_bar, int **out_FFTbuffer_lower_cut_off, int **out_FFTbuffer_upper_cut_off, double **out_eq) {

    int _number_of_bars = p->number_of_bars;

    *out_eq = malloc((_number_of_bars + 1) * sizeof(double));
    if (out_eq == NULL) {
        LOG_ERROR("[%p]: malloc for *out_eq failed", p);
        return false;
    }

    *out_FFTbuffer_lower_cut_off = malloc((_number_of_bars + 1) * sizeof(int));
    if (out_FFTbuffer_lower_cut_off == NULL) {
        LOG_ERROR("[%p]: malloc for *out_FFTbuffer_lower_cut_off failed", p);
        return false;
    }

    *out_FFTbuffer_upper_cut_off = malloc((_number_of_bars + 1) * sizeof(int));
    if (out_FFTbuffer_upper_cut_off == NULL) {
        LOG_ERROR("[%p]: malloc for *out_FFTbuffer_upper_cut_off failed", p);
        return false;
    }

    int *upper_cut_off_frequency = malloc((_number_of_bars + 1) * sizeof(int));
    int *center_frequencies = malloc((_number_of_bars + 1)* sizeof(int));
    int *relative_cut_off = malloc((_number_of_bars + 1) * sizeof(int));
    int *cut_off_frequency = malloc((_number_of_bars + 1)* sizeof(int));
    int *bar_buffer = malloc((_number_of_bars + 2) * sizeof(int));

    double frequency_constant;
    double bar_distribution_coefficient;
    bool first_bar = true;
    int first_treble_bar = 0;

    // in stereo ony half number of bars per channel
    if (p->channels == STEREO)
        _number_of_bars /= 2;

    frequency_constant = log10((float)p->lower_cut_off / (float)p->upper_cut_off)
                         / (1 / ((float)_number_of_bars + 1) - 1);

    for (int n = 0; n < _number_of_bars + 1; n++) {
        bar_distribution_coefficient = frequency_constant * (-1);
        bar_distribution_coefficient += ((float)n + 1) /((float)_number_of_bars + 1) * frequency_constant;

        cut_off_frequency[n] = (float)p->upper_cut_off * pow(10, bar_distribution_coefficient);

        if (n > 0) {
            if (cut_off_frequency[n - 1] >= cut_off_frequency[n]
                && cut_off_frequency[n - 1] > p->bass_cut_off) {
                cut_off_frequency[n] = cut_off_frequency[n - 1] + (cut_off_frequency[n - 1] - cut_off_frequency[n - 2]);
            }
        }

        // remember nyquist!, per my calculations this should be rate/2
        // and nyquist freq in M/2 but testing shows it is not...
        // or maybe the nq freq is in M/4
        relative_cut_off[n] = cut_off_frequency[n] / (p->sampling_rate / 2);
        
        (*out_eq)[n] = pow(cut_off_frequency[n], 1);

        // the numbers that come out of the FFT are verry high
        // the EQ is used to "normalize" them by dividing with this verry huge number
        (*out_eq)[n] *= (float)p->height / pow(2, 28);

        // if (p->userEQ)
        // ...

        (*out_eq)[n] /= log2(p->FFTbassbufferSize);

        if (cut_off_frequency[n] < p->bass_cut_off) {
            // BASS
            bar_buffer[n] = 1;
            (*out_FFTbuffer_lower_cut_off)[n] = relative_cut_off[n] * (p->FFTbassbufferSize / 2);
            (*out_bass_cut_off_bar)++;
            (*out_treble_cut_off_bar)++;
            if ((*out_bass_cut_off_bar)++ > 0)
                first_bar = false;

            (*out_eq)[n] *= log2(p->FFTbassbufferSize);
        }
        else if (cut_off_frequency[n] > p->bass_cut_off && cut_off_frequency[n] < p->treble_cut_off) {
            // MID
            bar_buffer[n] = 2;
            (*out_FFTbuffer_lower_cut_off)[n] = relative_cut_off[n] * (p->FFTmidbufferSize / 2);
            (*out_treble_cut_off_bar)++;
            if (((*out_treble_cut_off_bar) - (*out_bass_cut_off_bar)) == 1) {
                first_bar = true;
                (*out_FFTbuffer_upper_cut_off)[n - 1] = relative_cut_off[n] * (p->FFTbassbufferSize / 2);
            }
            else {
                first_bar = false;
            }

            (*out_eq)[n] *= log2(p->FFTmidbufferSize);
        }
        else {
            // TREBLE
            bar_buffer[n] = 3;
            (*out_FFTbuffer_lower_cut_off)[n] = relative_cut_off[n] * (p->FFTtreblebufferSize / 2);
            first_treble_bar++;
            if (first_treble_bar == 1) {
                first_bar = true;
                (*out_FFTbuffer_upper_cut_off)[n - 1] = relative_cut_off[n] * (p->FFTmidbufferSize / 2);
            }
            else {
                first_bar = false;
            }

            (*out_eq)[n] *= log2(p->FFTtreblebufferSize);
        }

        if (n > 0) {
            if (!first_bar) {
                (*out_FFTbuffer_upper_cut_off)[n - 1] = (*out_FFTbuffer_lower_cut_off)[n] -1;

                // pushing the spectrum up if the exponential function gets "clumped" in the
                // bass and caluclating new cut off frequencies
                if ((*out_FFTbuffer_lower_cut_off)[n] <= (*out_FFTbuffer_lower_cut_off)[n - 1]) {

                    (*out_FFTbuffer_lower_cut_off)[n] = (*out_FFTbuffer_lower_cut_off)[n - 1] + 1;
                    (*out_FFTbuffer_upper_cut_off)[n - 1] = (*out_FFTbuffer_lower_cut_off)[n] - 1;

                    if (bar_buffer[n] == 1)
                        relative_cut_off[n] = (float)((*out_FFTbuffer_lower_cut_off)[n]) 
                                              / ((float)p->FFTbassbufferSize / 2);
                    else if (bar_buffer[n] == 2)
                        relative_cut_off[n] = (float)((*out_FFTbuffer_lower_cut_off)[n])
                                              / ((float)p->FFTmidbufferSize / 2);
                    else if (bar_buffer[n] == 3)
                        relative_cut_off[n] = (float)((*out_FFTbuffer_lower_cut_off)[n])
                                              / ((float)p->FFTtreblebufferSize / 2);

                    cut_off_frequency[n] = relative_cut_off[n] * ((float)p->sampling_rate / 2);
                }
                else {
                    if ((*out_FFTbuffer_upper_cut_off)[n - 1] <= (*out_FFTbuffer_lower_cut_off)[n - 1])
                        (*out_FFTbuffer_upper_cut_off)[n - 1] = (*out_FFTbuffer_lower_cut_off)[n - 1] + 1;
                }
                upper_cut_off_frequency[n - 1] = cut_off_frequency[n];
                center_frequencies[n - 1] = pow((cut_off_frequency[n - 1] * upper_cut_off_frequency[n - 1]), 0.5);
            }
        }
    }

    free(upper_cut_off_frequency);
    free(center_frequencies);
    free(relative_cut_off);
    free(cut_off_frequency);
    free(bar_buffer);

    return true;
}

static bool _separate_frequency_bands(struct hueaudio_s *p, int bass_cut_off_bar, int treble_cut_off_bar,
                                      int *FFTbuffer_lower_cut_off, int *FFTbuffer_upper_cut_off, double *eq, int **out_bars) {

    *out_bars = malloc(p->number_of_bars * sizeof(int));
    if (out_bars == NULL) {
        LOG_ERROR("[%p]: malloc for *out_bars failed", p);
        return false;
    }

    double temp;

    for (int n = 0; n < p->number_of_bars; n++) {
        temp = 0;

        // add up fft values within bands
        LOG_SDEBUG("VALUES: %d, %d", FFTbuffer_lower_cut_off[n], FFTbuffer_upper_cut_off[n]);
        for (int i = FFTbuffer_lower_cut_off[n]; i <= FFTbuffer_upper_cut_off[n]; i++) {
            if (n <= bass_cut_off_bar) 
               temp += hypot(p->out_bass_l[i][REAL], (p->out_bass_l)[i][IMG]);
 
            else if (n > bass_cut_off_bar && n <= treble_cut_off_bar)
               temp += hypot(p->out_mid_l[i][REAL], p->out_mid_l[i][IMG]);
 
            else if (n > treble_cut_off_bar)
                temp += hypot(p->out_treble_l[i][REAL], p->out_treble_l[i][IMG]);
        }

        // getting average, multiply with sens and eq
        temp /= FFTbuffer_upper_cut_off[n] - FFTbuffer_lower_cut_off[n] + 1;
        temp *= p->sense * eq[n];

        if (temp <= p->ignore)
            temp = 0;

        (*out_bars)[n] = temp;
    }

    return true;
}

static bool _monstercat_filter(struct hueaudio_s *p, int number_of_bars, int **bars) {
    int k;

    // process [smoothing]: monstercat-style "average"
    if (p->waves > 0) {
        for (int i = 0; i < number_of_bars; i++) { // waves
            (*bars)[i] = (*bars)[i] / 1.25;

            for (int j = i - 1; j >= 0; j--) {
                k = i - j;
                (*bars)[j] = max((*bars)[i] - pow(k, 2), (*bars)[j]);
            }
            for (int j = i + 1; j < number_of_bars; j++) {
                k = j - i;
                (*bars)[j] = max((*bars)[i] - pow(k, 2), (*bars)[j]);
            }
        }
    } else if (p->monstercat > 0) {
        for (int i = 0; i < number_of_bars; i++) {
            for (int j = i - 1; j >= 0; j--) {
                k = i - j;
                (*bars)[j] = max((*bars)[i] / pow(p->monstercat, k), (*bars)[j]);
            }
            for (int j = i + 1; j < number_of_bars; j++) {
                k = j - i;
                (*bars)[j] = max((*bars)[i] / pow(p->monstercat, k), (*bars)[j]);
            }
        }
    }

    return true;
}

static bool _process_sound_signal(struct hueaudio_s *p, int *bars_left, int *bars_right) {
    int minvalue = 0;
    int maxvalue = 0;

    int *bars_last = malloc(p->number_of_bars * sizeof(int));
    float *bars_peak = malloc(p->number_of_bars * sizeof(float));
    int *fall = malloc(p->number_of_bars * sizeof(int));
    int *bars_mem = malloc(p->number_of_bars * sizeof(int));

    for (int n = 0; n < p->number_of_bars; n++) {
        // mirroring stereo channels
        if (p->channels == STEREO) {
            if (n < p->number_of_bars / 2)
                p->brightness[n] = bars_left[p->number_of_bars / 2 - n - 1];
            else
                p->brightness[n] = bars_right[n - p->number_of_bars / 2];
        }
        else {
            p->brightness[n] = bars_left[n];
        }

        // smoothing falloff
        double g;
        g = p->gravity * ((float)p->height / 2160) * pow((60 / (float)p->sampling_rate), 2.5);
        if (g > 0) {
            if (p->brightness[n] < bars_last[n]) {
                p->brightness[n] = bars_peak[n] - (g * fall[n] * fall[n]);
                if (p->brightness[n] < 0)
                    p->brightness[n] = 0;
                fall[n]++;
            }
            else {
                bars_peak[n] = p->brightness[n];
                fall[n] = 0;
            }
            bars_last[n] = p->brightness[n];
        }

        // smoothing integral
        double integral;
        integral = p->integral * 1 / sqrt(log10((float)p->height / 10));
        if (integral > 0) {
            p->brightness[n] = bars_mem[n] * integral + p->brightness[n];
            bars_mem[n] = p->brightness[n];

            int diff = p->height - p->brightness[n];
            if (diff < 0)
                diff = 0;

            double div = 1 / (diff + 1);
            bars_mem[n] = bars_mem[n] * (1 - div / 20);
        }

        if (p->brightness[n] < minvalue) {
            minvalue = p->brightness[n];
        }
        if (p->brightness[n] > maxvalue) {
            maxvalue = p->brightness[n];
        }
        if (p->brightness[n] < 0) {
            LOG_ERROR("[%p]: negative bar value: %d", p, p->brightness[n]);
        }
        // zero values cause divided by zero segfault
        if (p->brightness[n] < 1) {
            LOG_DEBUG("[%p]: zero bar value");
            p->brightness[n] = 1;
        }

        // automatic sense adjustment
        if (p->autosense) {
            if (p->brightness[n] > p->height && p->senselow) {
                p->sense = p->sense * 0.98;
                p->senselow = false;
                p->first = false;
            }
        }
    }

    if (p->autosense && p->senselow) {
        p->sense = p->sense * 1.001;
        if (p->first)
            p->sense = p->sense * 1.1;
    }

    free(bars_last);
    free(bars_peak);
    free(fall);
    free(bars_mem);

    return true;
}

bool hue_analyze_audio(struct hueaudio_s *p) {

    // execute the fftw with the plans
    fftw_execute(p->p_bass_l);
    fftw_execute(p->p_mid_l);
    fftw_execute(p->p_treble_l);
    if (p->channels == STEREO) {
        fftw_execute(p->p_bass_r);
        fftw_execute(p->p_mid_r);
        fftw_execute(p->p_treble_r);
    }

    int *FFTbuffer_lower_cut_off;
    int *FFTbuffer_upper_cut_off;
    double *eq;

    int bass_cut_off_bar = -1;
    int treble_cut_off_bar = -1;
    _calculate_cutoff_and_eq(p, &bass_cut_off_bar, &treble_cut_off_bar, &FFTbuffer_lower_cut_off, &FFTbuffer_upper_cut_off, &eq);

    LOG_SDEBUG("[%p]: bass_cut_off_bar: %d, treble_cut_off_bar: %d", p, bass_cut_off_bar, treble_cut_off_bar);

    int *bars_left;
    int *bars_right;
    _separate_frequency_bands(p, bass_cut_off_bar, treble_cut_off_bar, FFTbuffer_lower_cut_off, FFTbuffer_upper_cut_off, eq, &bars_left);
    if (p->channels == STEREO) {
        _separate_frequency_bands(p, bass_cut_off_bar, treble_cut_off_bar, FFTbuffer_lower_cut_off, FFTbuffer_upper_cut_off, eq, &bars_right);
    }

    if (p->monstercat) {
        if (p->channels == STEREO) {
            _monstercat_filter(p, p->number_of_bars / 2, &bars_left);
            _monstercat_filter(p, p->number_of_bars / 2, &bars_right);
        }
        else {
            _monstercat_filter(p, p->number_of_bars, &bars_left);
        }
    }

    _process_sound_signal(p, bars_left, bars_right);

    free(FFTbuffer_lower_cut_off);
    free(FFTbuffer_upper_cut_off);
    free(eq);

    free(bars_left);
    if (p->channels == STEREO)
        free(bars_right);

    LOG_DEBUG("[%p]: BAR VALUES: %d, %d, %d", p, p->brightness[0], p->brightness[1], p->brightness[2]);

    return true;
}
