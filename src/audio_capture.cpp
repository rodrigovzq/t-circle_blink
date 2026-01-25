#include "audio_capture.h"
#include "config.h"
#include <Arduino.h>
#include "pin_config.h"
#include "Arduino_DriveBus_Library.h"

// =============================================================================
// Implementación - Captura de Audio I2S
// =============================================================================

#define I2S_DATA_BIT 16

static std::shared_ptr<Arduino_IIS_DriveBus> i2s_bus =
    std::make_shared<Arduino_HWIIS>(I2S_NUM_0, MSM261_BCLK, MSM261_WS, MSM261_DATA);
static std::unique_ptr<Arduino_IIS> microphone(new Arduino_MEMS(i2s_bus));

bool audio_init() {
    bool success = microphone->begin(
        I2S_MODE_MASTER,
        AD_IIS_DATA_IN,
        I2S_CHANNEL_FMT_ONLY_LEFT,
        I2S_DATA_BIT,
        SAMPLE_RATE
    );

    if (success) {
        Serial.printf("[Audio] I2S OK: %d Hz\n", SAMPLE_RATE);
    } else {
        Serial.println("[Audio] ERROR: No se pudo inicializar I2S");
    }

    return success;
}

bool audio_capture(int16_t* buffer) {
    Serial.println("[Audio] Grabando...");

    size_t samples_captured = 0;
    unsigned long start_time = millis();
    unsigned long duration_ms = AUDIO_DURATION_SEC * 1000;
    int last_second = -1;

    while (millis() - start_time < duration_ms) {
        int16_t sample;
        if (microphone->IIS_Read_Data((char*)&sample, sizeof(int16_t))) {
            if (samples_captured < AUDIO_SAMPLES) {
                buffer[samples_captured++] = sample;
            }
        }

        // Progreso cada segundo
        int current_second = (millis() - start_time) / 1000;
        if (current_second > last_second && current_second <= AUDIO_DURATION_SEC) {
            Serial.printf("[Audio] %d/%d seg\n", current_second, AUDIO_DURATION_SEC);
            last_second = current_second;
        }
    }

    Serial.printf("[Audio] Capturados: %d samples\n", samples_captured);

    if (samples_captured < AUDIO_SAMPLES * 0.9) {
        Serial.printf("[Audio] WARNING: Solo %.1f%% capturado\n",
                      samples_captured * 100.0f / AUDIO_SAMPLES);
    }

    return samples_captured > 0;
}

float audio_normalize(int16_t* buffer, float target_db) {
    // Encontrar pico máximo
    int16_t max_abs = 0;
    for (int i = 0; i < AUDIO_SAMPLES; i++) {
        int16_t abs_val = abs(buffer[i]);
        if (abs_val > max_abs) max_abs = abs_val;
    }

    if (max_abs == 0) {
        Serial.println("[Audio] WARNING: Silencio total");
        return 1.0f;
    }

    // Calcular ganancia para alcanzar target_db
    float target_linear = pow(10.0f, target_db / 20.0f);
    int16_t target_peak = (int16_t)(target_linear * 32767);
    float gain = (float)target_peak / max_abs;

    // Aplicar ganancia con protección de clipping
    for (int i = 0; i < AUDIO_SAMPLES; i++) {
        int32_t scaled = (int32_t)(buffer[i] * gain);
        buffer[i] = (int16_t)constrain(scaled, -32768, 32767);
    }

    Serial.printf("[Audio] Normalizado: pico %d -> %d (x%.2f)\n", max_abs, target_peak, gain);

    return gain;
}

AudioStats audio_get_stats(const int16_t* buffer) {
    AudioStats stats = {0, INT16_MIN, INT16_MAX, 0};
    int64_t sum_squared = 0;
    int16_t prev_sample = 0;

    for (int i = 0; i < AUDIO_SAMPLES; i++) {
        int16_t sample = buffer[i];
        sum_squared += (int64_t)sample * sample;

        if (sample > stats.peak_pos) stats.peak_pos = sample;
        if (sample < stats.peak_neg) stats.peak_neg = sample;

        // Zero crossing
        if (i > 0 && ((prev_sample >= 0 && sample < 0) || (prev_sample < 0 && sample >= 0))) {
            stats.zero_crossings++;
        }
        prev_sample = sample;
    }

    stats.rms = sqrt(sum_squared / (double)AUDIO_SAMPLES);

    return stats;
}
