#include "mfcc_extractor.h"
#include "config.h"
#include <Arduino.h>
#include <math.h>
#include "esp_heap_caps.h"
#include "arduinoFFT.h"

// =============================================================================
// Implementación - Extracción de MFCCs
// =============================================================================

// Buffers internos (alocados en PSRAM)
static float* vReal = nullptr;
static float* vImag = nullptr;
static float* melFilterbank = nullptr;
static float* dctMatrix = nullptr;
static float* hammingWindow = nullptr;

static ArduinoFFT<float> FFT;

static const int MEL_COLS = (N_FFT / 2) + 1;

// -----------------------------------------------------------------------------
// Funciones internas
// -----------------------------------------------------------------------------

static void init_hamming_window() {
    for (int i = 0; i < N_FFT; i++) {
        hammingWindow[i] = 0.54f - 0.46f * cos(2.0f * PI * i / (N_FFT - 1));
    }
}

static void init_mel_filterbank() {
    auto hzToMel = [](float hz) { return 2595.0f * log10(1.0f + hz / 700.0f); };
    auto melToHz = [](float mel) { return 700.0f * (pow(10.0f, mel / 2595.0f) - 1.0f); };

    float melMin = hzToMel(0);
    float melMax = hzToMel(SAMPLE_RATE / 2.0f);

    float melPoints[N_MELS + 2];
    for (int i = 0; i < N_MELS + 2; i++) {
        melPoints[i] = melMin + (melMax - melMin) * i / (N_MELS + 1);
    }

    int bins[N_MELS + 2];
    for (int i = 0; i < N_MELS + 2; i++) {
        float hz = melToHz(melPoints[i]);
        bins[i] = (int)floor((N_FFT + 1) * hz / SAMPLE_RATE);
    }

    memset(melFilterbank, 0, N_MELS * MEL_COLS * sizeof(float));

    for (int m = 0; m < N_MELS; m++) {
        int leftBin = bins[m];
        int centerBin = bins[m + 1];
        int rightBin = bins[m + 2];

        for (int k = leftBin; k < centerBin; k++) {
            if (k < MEL_COLS) {
                melFilterbank[m * MEL_COLS + k] = (float)(k - leftBin) / (centerBin - leftBin);
            }
        }

        for (int k = centerBin; k < rightBin; k++) {
            if (k < MEL_COLS) {
                melFilterbank[m * MEL_COLS + k] = (float)(rightBin - k) / (rightBin - centerBin);
            }
        }
    }
}

static void init_dct_matrix() {
    for (int i = 0; i < N_MFCC; i++) {
        for (int j = 0; j < N_MELS; j++) {
            dctMatrix[i * N_MELS + j] = cos(PI * i * (j + 0.5f) / N_MELS) * sqrt(2.0f / N_MELS);
        }
    }
}

static void extract_frame(const int16_t* audio, int frame_index, float* mfccs) {
    int offset = frame_index * HOP_LENGTH;

    // 1. Aplicar ventana Hamming
    for (int i = 0; i < N_FFT; i++) {
        if (offset + i < AUDIO_SAMPLES) {
            vReal[i] = audio[offset + i] * hammingWindow[i];
        } else {
            vReal[i] = 0;
        }
        vImag[i] = 0;
    }

    // 2. FFT
    FFT.compute(vReal, vImag, N_FFT, FFTDirection::Forward);

    // 3. Magnitud (reusar vReal)
    for (int i = 0; i < MEL_COLS; i++) {
        vReal[i] = sqrt(vReal[i] * vReal[i] + vImag[i] * vImag[i]);
    }

    // 4. Mel filterbank (reusar vImag para mel energies)
    for (int m = 0; m < N_MELS; m++) {
        float energy = 0.0f;
        for (int k = 0; k < MEL_COLS; k++) {
            energy += vReal[k] * melFilterbank[m * MEL_COLS + k];
        }
        vImag[m] = log(energy + 1e-10f);
    }

    // 5. DCT -> MFCCs
    for (int i = 0; i < N_MFCC; i++) {
        float sum = 0.0f;
        for (int j = 0; j < N_MELS; j++) {
            sum += vImag[j] * dctMatrix[i * N_MELS + j];
        }
        mfccs[i] = sum;
    }
}

// -----------------------------------------------------------------------------
// API pública
// -----------------------------------------------------------------------------

bool mfcc_init() {
    Serial.println("[MFCC] Inicializando...");

    // Alocar buffers en PSRAM
    vReal = (float*)heap_caps_aligned_alloc(16, N_FFT * sizeof(float), MALLOC_CAP_SPIRAM);
    vImag = (float*)heap_caps_aligned_alloc(16, N_FFT * sizeof(float), MALLOC_CAP_SPIRAM);
    melFilterbank = (float*)heap_caps_aligned_alloc(16, N_MELS * MEL_COLS * sizeof(float), MALLOC_CAP_SPIRAM);
    dctMatrix = (float*)heap_caps_aligned_alloc(16, N_MFCC * N_MELS * sizeof(float), MALLOC_CAP_SPIRAM);
    hammingWindow = (float*)heap_caps_aligned_alloc(16, N_FFT * sizeof(float), MALLOC_CAP_SPIRAM);

    if (!vReal || !vImag || !melFilterbank || !dctMatrix || !hammingWindow) {
        Serial.println("[MFCC] ERROR: No se pudo alocar memoria");
        return false;
    }

    // Inicializar matrices
    init_hamming_window();
    init_mel_filterbank();
    init_dct_matrix();

    Serial.printf("[MFCC] OK: %d MFCCs x %d frames\n", N_MFCC, N_FRAMES);

    return true;
}

void mfcc_extract(const int16_t* audio_in, float* mfcc_out) {
    Serial.println("[MFCC] Extrayendo...");

    float frameMFCCs[N_MFCC];

    for (int frame = 0; frame < N_FRAMES; frame++) {
        extract_frame(audio_in, frame, frameMFCCs);

        // Normalizar y guardar en formato (N_MFCC, N_FRAMES)
        for (int i = 0; i < N_MFCC; i++) {
            mfcc_out[i * N_FRAMES + frame] = (frameMFCCs[i] - MFCC_MEAN) / MFCC_STD;
        }

        if (frame % 25 == 0) {
            Serial.printf("[MFCC] Frame %d/%d\n", frame, N_FRAMES);
        }
    }

    Serial.println("[MFCC] Completado");
}

void mfcc_deinit() {
    if (vReal) heap_caps_free(vReal);
    if (vImag) heap_caps_free(vImag);
    if (melFilterbank) heap_caps_free(melFilterbank);
    if (dctMatrix) heap_caps_free(dctMatrix);
    if (hammingWindow) heap_caps_free(hammingWindow);

    vReal = vImag = melFilterbank = dctMatrix = hammingWindow = nullptr;
}
