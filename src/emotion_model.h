#ifndef EMOTION_MODEL_H
#define EMOTION_MODEL_H

#include <stdint.h>
#include <stddef.h>

// =============================================================================
// Modelo de Reconocimiento de Emociones (TFLite)
// =============================================================================

struct EmotionResult {
    const char* label;          // Nombre de la emoción detectada
    float confidence;           // Confianza (0.0 - 1.0)
    int index;                  // Índice de la emoción (0-6)
    float probabilities[7];     // Probabilidades de todas las emociones
};

// Carga el modelo desde LittleFS
// path: ruta del archivo .tflite (ej: "/modelo.tflite")
// Retorna true si OK
bool model_load(const char* path);

// Ejecuta inferencia sobre los MFCCs
// mfcc_in: buffer de N_MFCC * N_FRAMES floats
// Retorna el resultado de la inferencia
EmotionResult model_predict(const float* mfcc_in);

// Libera memoria del modelo (opcional)
void model_unload();

// Retorna el tamaño del modelo en bytes
size_t model_get_size_bytes();

// Retorna el tamaño del tensor arena en bytes
size_t model_get_arena_size_bytes();

#endif // EMOTION_MODEL_H
