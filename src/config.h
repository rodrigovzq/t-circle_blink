#ifndef CONFIG_H
#define CONFIG_H

// =============================================================================
// MoodLink - Configuración del Pipeline
// =============================================================================
// Estos valores están definidos por el modelo y dataset de entrenamiento.
// NO modificar sin reentrenar el modelo.
// =============================================================================

#define VERSION "v5.1-modular-pipeline"

// -----------------------------------------------------------------------------
// Modelo
// -----------------------------------------------------------------------------
constexpr const char* MODEL_PATH = "/ser_202601_optimized_int8.tflite";

// -----------------------------------------------------------------------------
// Audio (definido por el dataset de entrenamiento)
// -----------------------------------------------------------------------------
constexpr int SAMPLE_RATE = 44100;
constexpr int AUDIO_DURATION_SEC = 4;
constexpr int AUDIO_SAMPLES = SAMPLE_RATE * AUDIO_DURATION_SEC;  // 176400

// -----------------------------------------------------------------------------
// MFCC (definido por el preprocesamiento del modelo)
// -----------------------------------------------------------------------------
constexpr int N_MFCC = 40;
constexpr int N_FRAMES = 100;
constexpr int N_MELS = 40;
constexpr int N_FFT = 2048;
constexpr int HOP_LENGTH = (AUDIO_SAMPLES - N_FFT) / (N_FRAMES - 1);  // ~1762

// Normalización de MFCCs (valores del dataset de entrenamiento)
constexpr float MFCC_MEAN = -8.0306f;
constexpr float MFCC_STD = 82.2183f;

// -----------------------------------------------------------------------------
// Inferencia
// -----------------------------------------------------------------------------
constexpr int NUM_EMOTIONS = 7;
constexpr const char* EMOTION_LABELS[] = {
    "anger", "disgust", "fear", "happy", "neutral", "sad", "surprise"
};

// -----------------------------------------------------------------------------
// Hardware - Micrófono I2S (T-Circle S3)
// -----------------------------------------------------------------------------
// Los pines están definidos en pin_config.h:
// MSM261_BCLK = GPIO7, MSM261_WS = GPIO9, MSM261_DATA = GPIO8

#endif // CONFIG_H
