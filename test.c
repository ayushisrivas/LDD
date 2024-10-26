#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define SAMPLE_RATE 44100
#define CHANNELS    2
#define DURATION    5  // seconds
#define FREQ        440 // Hz (A4 note)

// Test tone generation
void generate_sine_wave(int16_t *buffer, int samples) {
    for (int i = 0; i < samples; i++) {
        double time = (double)i / SAMPLE_RATE;
        double value = sin(2.0 * M_PI * FREQ * time);
        
        // Scale to 16-bit range and store in both channels
        int16_t sample = (int16_t)(value * 32767);
        buffer[i * 2] = sample;     // Left channel
        buffer[i * 2 + 1] = sample; // Right channel
    }
}

// Playback test
int test_playback() {
    snd_pcm_t *handle;
    int err;
    
    // Open PCM device
    if ((err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        return err;
    }

    // Set hardware parameters
    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    
    err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    err |= snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    err |= snd_pcm_hw_params_set_channels(handle, params, CHANNELS);
    unsigned int rate = SAMPLE_RATE;
    err |= snd_pcm_hw_params_set_rate_near(handle, params, &rate, 0);
    
    if ((err = snd_pcm_hw_params(handle, params)) < 0) {
        printf("Hardware parameter setting failed: %s\n", snd_strerror(err));
        snd_pcm_close(handle);
        return err;
    }

    // Generate and play test tone
    int samples = SAMPLE_RATE * DURATION;
    int16_t *buffer = malloc(samples * CHANNELS * sizeof(int16_t));
    generate_sine_wave(buffer, samples);
    
    printf("Playing %dHz test tone for %d seconds...\n", FREQ, DURATION);
    
    int frames = samples;
    while (frames > 0) {
        err = snd_pcm_writei(handle, buffer + (samples - frames) * CHANNELS, frames);
        if (err == -EPIPE) {
            printf("Buffer underrun, recovering...\n");
            snd_pcm_prepare(handle);
        } else if (err < 0) {
            printf("Write error: %s\n", snd_strerror(err));
            break;
        }
        frames -= err;
    }

    free(buffer);
    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    return 0;
}

// Recording test
int test_recording() {
    snd_pcm_t *handle;
    int err;
    
    // Open PCM device for recording
    if ((err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        printf("Recording open error: %s\n", snd_strerror(err));
        return err;
    }

    // Set hardware parameters
    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    
    err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    err |= snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    err |= snd_pcm_hw_params_set_channels(handle, params, CHANNELS);
    unsigned int rate = SAMPLE_RATE;
    err |= snd_pcm_hw_params_set_rate_near(handle, params, &rate, 0);
    
    if ((err = snd_pcm_hw_params(handle, params)) < 0) {
        printf("Hardware parameter setting failed: %s\n", snd_strerror(err));
        snd_pcm_close(handle);
        return err;
    }

    // Record audio
    int samples = SAMPLE_RATE * DURATION;
    int16_t *buffer = malloc(samples * CHANNELS * sizeof(int16_t));
    
    printf("Recording for %d seconds...\n", DURATION);
    
    int frames = samples;
    while (frames > 0) {
        err = snd_pcm_readi(handle, buffer + (samples - frames) * CHANNELS, frames);
        if (err == -EPIPE) {
            printf("Buffer overrun, recovering...\n");
            snd_pcm_prepare(handle);
        } else if (err < 0) {
            printf("Read error: %s\n", snd_strerror(err));
            break;
        }
        frames -= err;
    }

    // Save recorded audio to file
    FILE *f = fopen("test_recording.raw", "wb");
    if (f) {
        fwrite(buffer, sizeof(int16_t), samples * CHANNELS, f);
        fclose(f);
        printf("Recording saved to test_recording.raw\n");
    }

    free(buffer);
    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    return 0;
}

int main(int argc, char *argv[]) {
    printf("ALSA USB Audio Test Suite\n");
    printf("1. Testing playback...\n");
    test_playback();
    
    printf("\n2. Testing recording...\n");
    test_recording();
    
    return 0;
}
