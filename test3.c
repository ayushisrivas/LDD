// [Previous code remains the same until process_recording function]

int process_recording(const char *filename) {
    printf("\n=== Processing Recording ===\n");
    
    // Open and check file
    FILE *f = fopen(filename, "rb");
    if (!f) {
        printf("Error: Cannot open recorded file %s\n", filename);
        return -1;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    printf("File size: %ld bytes\n", file_size);
    
    if (file_size == 0) {
        printf("Error: Recording file is empty\n");
        fclose(f);
        return -1;
    }
    
    // Allocate buffer with error checking
    int16_t *buffer = malloc(file_size);
    if (!buffer) {
        printf("Error: Memory allocation failed\n");
        fclose(f);
        return -1;
    }
    
    // Read file with verification
    size_t read_size = fread(buffer, 1, file_size, f);
    fclose(f);
    
    if (read_size != file_size) {
        printf("Error: Could not read entire file. Read %zu of %ld bytes\n", read_size, file_size);
        free(buffer);
        return -1;
    }
    
    printf("Successfully read %zu bytes from recording\n", read_size);
    
    // Analyze audio
    AudioStats stats = analyze_audio(buffer, read_size / sizeof(int16_t));
    
    // Print analysis with error checking for invalid values
    printf("\nAudio Analysis Results:\n");
    if (stats.peak_amplitude > 0) {
        printf("Peak Level: %.2f dB\n", 20 * log10(stats.peak_amplitude));
        printf("Average Level: %.2f dB\n", 20 * log10(stats.average_amplitude));
        printf("RMS Level: %.2f dB\n", 20 * log10(stats.rms_level));
    } else {
        printf("Warning: Invalid audio levels detected\n");
    }
    printf("Clipping Detected: %d instances\n", stats.clipping_count);
    
    // Save metadata with error checking
    char metadata_filename[256];
    snprintf(metadata_filename, sizeof(metadata_filename), "%s.meta", filename);
    f = fopen(metadata_filename, "w");
    if (!f) {
        printf("Error: Cannot create metadata file\n");
        free(buffer);
        return -1;
    }
    
    fprintf(f, "Recording Metadata:\n");
    fprintf(f, "Sample Rate: %d Hz\n", SAMPLE_RATE);
    fprintf(f, "Channels: %d\n", CHANNELS);
    fprintf(f, "Duration: %d seconds\n", DURATION);
    fprintf(f, "File Size: %ld bytes\n", file_size);
    fprintf(f, "Peak Level: %.2f dB\n", 20 * log10(stats.peak_amplitude));
    fprintf(f, "Average Level: %.2f dB\n", 20 * log10(stats.average_amplitude));
    fprintf(f, "RMS Level: %.2f dB\n", 20 * log10(stats.rms_level));
    fprintf(f, "Clipping Instances: %d\n", stats.clipping_count);
    fclose(f);
    printf("\nMetadata saved to %s\n", metadata_filename);
    
    // Playback verification with robust error handling
    printf("\n=== Verifying Recording Playback ===\n");
    snd_pcm_t *handle;
    int err;
    
    if ((err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        printf("Error: Playback open failed: %s\n", snd_strerror(err));
        free(buffer);
        return -1;
    }
    
    // Set hardware parameters
    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    
    // Configure parameters with error checking
    if ((err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        printf("Error setting access: %s\n", snd_strerror(err));
        goto cleanup;
    }
    
    if ((err = snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE)) < 0) {
        printf("Error setting format: %s\n", snd_strerror(err));
        goto cleanup;
    }
    
    if ((err = snd_pcm_hw_params_set_channels(handle, params, CHANNELS)) < 0) {
        printf("Error setting channels: %s\n", snd_strerror(err));
        goto cleanup;
    }
    
    unsigned int rate = SAMPLE_RATE;
    if ((err = snd_pcm_hw_params_set_rate_near(handle, params, &rate, 0)) < 0) {
        printf("Error setting rate: %s\n", snd_strerror(err));
        goto cleanup;
    }
    
    // Apply hardware parameters
    if ((err = snd_pcm_hw_params(handle, params)) < 0) {
        printf("Error setting hardware parameters: %s\n", snd_strerror(err));
        goto cleanup;
    }
    
    printf("Playing back recording...\n");
    printf("Press Ctrl+C to stop playback\n");
    
    // Calculate frames
    size_t frames = read_size / (CHANNELS * sizeof(int16_t));
    size_t pos = 0;
    size_t chunk_size = BUFFER_SIZE; // Use smaller chunks for smoother playback
    
    // Playback loop with progress indication
    while (pos < frames) {
        size_t frames_to_write = (frames - pos < chunk_size) ? frames - pos : chunk_size;
        
        err = snd_pcm_writei(handle, buffer + pos * CHANNELS, frames_to_write);
        
        if (err == -EPIPE) {
            printf("Buffer underrun, recovering...\n");
            snd_pcm_prepare(handle);
            continue;
        } else if (err < 0) {
            printf("Write error: %s\n", snd_strerror(err));
            break;
        }
        
        pos += err;
        
        // Show progress
        printf("\rPlayback progress: %.1f%%", (float)pos * 100 / frames);
        fflush(stdout);
    }
    
    printf("\nPlayback completed!\n");
    
cleanup:
    if (handle) {
        snd_pcm_drain(handle);
        snd_pcm_close(handle);
    }
    free(buffer);
    return (err < 0) ? -1 : 0;
}

int main() {
    printf("=== ALSA Audio Test Suite ===\n");
    
    // Initialize mixer with error checking
    printf("\nInitializing mixer controls...\n");
    if (setup_mixer_controls() < 0) {
        printf("Failed to initialize mixer controls\n");
        return 1;
    }
    
    // Test playback with error checking
    printf("\nStarting playback test...\n");
    if (test_playback() < 0) {
        printf("Playback test failed\n");
        return 1;
    }
    
    // Wait between tests
    printf("\nWaiting 2 seconds before recording...\n");
    sleep(2);
    
    // Test recording with error checking
    const char *recording_file = "test_recording.raw";
    printf("\nStarting recording test...\n");
    if (test_recording(recording_file) < 0) {
        printf("Recording test failed\n");
        return 1;
    }
    
    // Process and verify recording
    printf("\nStarting recording verification...\n");
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
