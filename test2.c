#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#define SAMPLE_RATE 44100
#define CHANNELS    2
#define DURATION    5  // seconds
#define FREQ        440 // Hz (A4 note)
#define BUFFER_SIZE 1024

typedef struct {
    double peak_amplitude;
    double average_amplitude;
    int clipping_count;
    double rms_level;
} AudioStats;

// Initialize ALSA mixer
int setup_mixer_controls() {
    snd_mixer_t *mixer;
    int err;
    
    if ((err = snd_mixer_open(&mixer, 0)) < 0) {
        printf("Mixer open error: %s\n", snd_strerror(err));
        return -1;
    }
    
    if ((err = snd_mixer_attach(mixer, "default")) < 0) {
        printf("Mixer attach error: %s\n", snd_strerror(err));
        snd_mixer_close(mixer);
        return -1;
    }
    
    if ((err = snd_mixer_selem_register(mixer, NULL, NULL)) < 0) {
        printf("Mixer register error: %s\n", snd_strerror(err));
        snd_mixer_close(mixer);
        return -1;
    }
    
    if ((err = snd_mixer_load(mixer)) < 0) {
        printf("Mixer load error: %s\n", snd_strerror(err));
        snd_mixer_close(mixer);
        return -1;
    }
    
    // Set initial volume
    snd_mixer_elem_t *elem;
    snd_mixer_selem_id_t *sid;
    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_name(sid, "Master");
    
    elem = snd_mixer_find_selem(mixer, sid);
    if (elem) {
        long min, max;
        snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
        snd_mixer_selem_set_playback_volume_all(elem, max * 0.8); // 80% volume
    }
    
    snd_mixer_close(mixer);
    return 0;
}

// Generate test tone
void generate_sine_wave(int16_t *buffer, int samples) {
    for (int i = 0; i < samples; i++) {
        double time = (double)i / SAMPLE_RATE;
        double value = sin(2.0 * M_PI * FREQ * time);
        int16_t sample = (int16_t)(value * 32767 * 0.5); // 50% amplitude
        buffer[i * 2] = sample;     // Left channel
        buffer[i * 2 + 1] = sample; // Right channel
    }
}

// Test playback functionality
int test_playback() {
    printf("\n=== Testing Playback ===\n");
    snd_pcm_t *handle;
    int err;
    
    if ((err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        return err;
    }
    
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

// Test recording functionality
int test_recording(const char *filename) {
    printf("\n=== Testing Recording ===\n");
    snd_pcm_t *handle;
    int err;
    
    if ((err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        printf("Recording open error: %s\n", snd_strerror(err));
        return err;
    }
    
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
    
    printf("Recording for %d seconds...\n", DURATION);
    printf("Please make some noise!\n");
    
    int samples = SAMPLE_RATE * DURATION;
    int16_t *buffer = malloc(samples * CHANNELS * sizeof(int16_t));
    
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
    
    FILE *f = fopen(filename, "wb");
    if (f) {
        fwrite(buffer, sizeof(int16_t), samples * CHANNELS, f);
        fclose(f);
        printf("Recording saved to %s\n", filename);
    }
    
    free(buffer);
    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    return 0;
}

// Analyze recorded audio
AudioStats analyze_audio(int16_t *buffer, size_t buffer_size) {
    AudioStats stats = {0};
    double sum = 0;
    double squared_sum = 0;
    
    for (size_t i = 0; i < buffer_size; i++) {
        double amplitude = fabs((double)buffer[i] / 32768.0);
        sum += amplitude;
        squared_sum += amplitude * amplitude;
        
        if (amplitude > stats.peak_amplitude) {
            stats.peak_amplitude = amplitude;
        }
        
        if (abs(buffer[i]) >= 32767) {
            stats.clipping_count++;
        }
    }
    
    stats.average_amplitude = sum / buffer_size;
    stats.rms_level = sqrt(squared_sum / buffer_size);
    
    return stats;
}

// Verify and analyze recording
int process_recording(const char *filename) {
    printf("\n=== Processing Recording ===\n");
    
    FILE *f = fopen(filename, "rb");
    if (!f) {
        printf("Error opening recorded file\n");
        return -1;
    }
    
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    int16_t *buffer = malloc(file_size);
    size_t read_size = fread(buffer, 1, file_size, f);
    fclose(f);
    
    // Analyze audio
    AudioStats stats = analyze_audio(buffer, read_size / sizeof(int16_t));
    
    // Print analysis
    printf("\nAudio Analysis Results:\n");
    printf("Peak Level: %.2f dB\n", 20 * log10(stats.peak_amplitude));
    printf("Average Level: %.2f dB\n", 20 * log10(stats.average_amplitude));
    printf("RMS Level: %.2f dB\n", 20 * log10(stats.rms_level));
    printf("Clipping Detected: %d instances\n", stats.clipping_count);
    
    // Save metadata
    char metadata_filename[256];
    snprintf(metadata_filename, sizeof(metadata_filename), "%s.meta", filename);
    f = fopen(metadata_filename, "w");
    if (f) {
        fprintf(f, "Recording Metadata:\n");
        fprintf(f, "Sample Rate: %d Hz\n", SAMPLE_RATE);
        fprintf(f, "Channels: %d\n", CHANNELS);
        fprintf(f, "Duration: %d seconds\n", DURATION);
        fprintf(f, "Peak Level: %.2f dB\n", 20 * log10(stats.peak_amplitude));
        fprintf(f, "Average Level: %.2f dB\n", 20 * log10(stats.average_amplitude));
        fprintf(f, "RMS Level: %.2f dB\n", 20 * log10(stats.rms_level));
        fprintf(f, "Clipping Instances: %d\n", stats.clipping_count);
        fclose(f);
        printf("\nMetadata saved to %s\n", metadata_filename);
    }
    
    // Playback verification
    printf("\n=== Verifying Recording Playback ===\n");
    snd_pcm_t *handle;
    int err;
    
    if ((err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        free(buffer);
        return -1;
    }
    
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
        free(buffer);
        return -1;
    }
    
    printf("Playing back recording...\n");
    
    size_t frames = read_size / (CHANNELS * sizeof(int16_t));
    size_t pos = 0;
    
    while (pos < frames) {
        err = snd_pcm_writei(handle, buffer + pos * CHANNELS, frames - pos);
        if (err == -EPIPE) {
            printf("Buffer underrun, recovering...\n");
            snd_pcm_prepare(handle);
        } else if (err < 0) {
            printf("Write error: %s\n", snd_strerror(err));
            break;
        }
        pos += err;
    }
    
    free(buffer);
    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    return 0;
}

int main() {
    printf("=== ALSA Audio Test Suite ===\n");
    
    // Initialize mixer
    printf("\nInitializing mixer controls...\n");
    if (setup_mixer_controls() < 0) {
        printf("Failed to initialize mixer controls\n");
        return 1;
    }
    
    // Test playback
    if (test_playback() < 0) {
        printf("Playback test failed\n");
        return 1;
    }
    
    // Wait a moment between playback and recording
    printf("\nWaiting 2 seconds before recording...\n");
    sleep(2);
    
    // Test recording
    const char *recording_file = "test_recording.raw";
    if (test_recording(recording_file) < 0) {
        printf("Recording test failed\n");
        return 1;
    }
    
    // Process and verify recording
    if (process_recording(recording_file) < 0) {
        printf("Recording processing failed\n");
        return 1;
    }
    
    printf("\n=== Test Suite Complete ===\n");
    printf("Files generated:\n");
    printf("1. %s (Raw audio data)\n", recording_file);
    printf("2. %s.meta (Recording metadata)\n", recording_file);
    
    return 0;
}
