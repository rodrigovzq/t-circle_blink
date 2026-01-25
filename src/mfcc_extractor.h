#ifndef MFCC_EXTRACTOR_H
#define MFCC_EXTRACTOR_H

#include <stdint.h>
#include <stddef.h>

// =============================================================================
// Extracción de MFCCs
// =============================================================================

// Inicializa las matrices necesarias (Hamming, Mel filterbank, DCT)
// Aloca memoria interna en PSRAM
// Retorna true si OK
bool mfcc_init();

// Extrae MFCCs del audio
// audio_in: buffer de AUDIO_SAMPLES muestras int16_t
// mfcc_out: buffer de N_MFCC * N_FRAMES floats (debe estar pre-alocado)
void mfcc_extract(const int16_t* audio_in, float* mfcc_out);

// Libera memoria interna (opcional, para cleanup)
void mfcc_deinit();

// Retorna la cantidad de memoria interna usada (en bytes)
size_t mfcc_get_internal_memory_bytes();

#endif // MFCC_EXTRACTOR_H
