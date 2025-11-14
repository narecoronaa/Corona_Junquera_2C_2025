/* drum_samples.h */
#ifndef DRUM_SAMPLES_H
#define DRUM_SAMPLES_H

#include <stdint.h> // Para que reconozca el tipo int16_t

/* * Declaramos los arrays y sus tamaños como "extern".
 * Esto le dice al compilador: "Estas variables existen,
 * pero su definición está en otro archivo .c".
 */

// Sample para el PAD A (Snare)
extern const int snare_drum_size;
extern const int16_t snare_drum_sample[];

// Sample para el PAD B (Hi-Hat)
extern const int hi_hat_size;
extern const int16_t hi_hat_sample[];

#endif // DRUM_SAMPLES_H