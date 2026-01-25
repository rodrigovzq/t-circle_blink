#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H

#include <stdint.h>

// =============================================================================
// Captura de Audio I2S
// =============================================================================

struct AudioStats {
    float rms;
    int16_t peak_pos;
    int16_t peak_neg;
    int zero_crossings;
};

// Inicializa el micrófono I2S
// Retorna true si OK
bool audio_init();

// Captura audio del micrófono y lo escribe en el buffer
// El buffer debe tener espacio para AUDIO_SAMPLES muestras
// Retorna true si la captura fue exitosa
bool audio_capture(int16_t* buffer);

// Normaliza el audio a un nivel target en dB (ej: -1.0f)
// Modifica el buffer in-place
// Retorna el factor de ganancia aplicado
float audio_normalize(int16_t* buffer, float target_db = -1.0f);

// Calcula estadísticas del audio
AudioStats audio_get_stats(const int16_t* buffer);

#endif // AUDIO_CAPTURE_H
