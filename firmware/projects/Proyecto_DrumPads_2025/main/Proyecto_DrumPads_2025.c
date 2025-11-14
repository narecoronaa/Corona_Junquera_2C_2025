/*! @mainpage Adquisición de señal piezoeléctrica por ADC y envío por UART
 *
 * @section genDesc Descripción General
 *
 * Esta aplicación adquiere la señal analógica de dos sensores piezoeléctricos
 * (PAD A y PAD B) conectados a los canales CH1 y CH0 del ADC. La señal se muestrea
 * periódicamente. Cuando un golpe supera un umbral, se reproduce un sonido
 * específico (Snare o Hi-Hat) por el DAC.
 *
 * Características principales:
 * - Sensores: 2 Piezoeléctricos (PAD A -> CH1, PAD B -> CH0).
 * - Muestreo: 20 kHz (50 μs) (Ver nota de rendimiento).
 * - Detección: Por umbral en la tarea de ADC.
 * - Salida de audio: DAC (Buzzer/Audio Out).
 * - Feedback visual: LED Neopixel.
 * - Implementación: 
 * - Timer notifica a AdcTask.
 * - AdcTask lee ADCs, compara con umbral y envía datos por UART.
 * - Si hay golpe, AdcTask notifica a UmbralTask (LED) y a PlaySoundTask (Audio).
 * - PlaySoundTask es una tarea única que reproduce el sonido correspondiente.
 *
 * @section hardConn Conexión de Hardware
 *
 * |    Peripheral  |   ESP32   	|
 * |:--------------:|:--------------|
 * | 	CH1 ADC (PAD A)| 	GPIO_1		|
 * | 	CH0 ADC (PAD B)| 	GPIO_0		| // Ver si usamos ese
 * | 	AUDIO_OUT 	 | 	GPIO_4		|
 * | 	UART_PC	 	 | 	USB			|
 * |    RGB LED      |   BUILT_IN_RGB_LED_PIN |
 *
 * @author Corona Narella (narella.corona@ingenieria.uner.edu.ar)
 * @date 31/10/2025
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "timer_mcu.h"
#include "uart_mcu.h"
#include "analog_io_mcu.h"
#include "neopixel_stripe.h"
#include "gpio_mcu.h"
#include "drum_samples.h" 

/*==================[macros and definitions]=================================*/
/** Período del timer A para muestreo ADC: 20KHz = 50μs */
#define TIMER_ADC_PERIOD_US     50

/** Canal ADC a usar para PAD A */
#define ADC_CHANNEL_A            CH1

/** Canal ADC a usar para PAD B */
#define ADC_CHANNEL_B            CH0

/** Velocidad UART para transmisión */
#define UART_BAUD_RATE          921600

/** Umbral para la detección del evento*/
#define ADC_THRESHOLD_MV_MINIMUM        400

/** Pin de salida para el buzzer/audio */
#define GPIO_AUDIO_OUT          GPIO_4  

/** Frecuencia de muestreo utilizada en la señal de audio (DAC) */
#define SAMPLE_RATE             8000

/** Cooldown para evitar múltiples disparos del mismo golpe (en milisegundos) */
#define HIT_COOLDOWN_MS         100 

// CAMBIO: Definimos valores para las notificaciones de sonido
#define PLAY_SNARE              1
#define PLAY_HIHAT              2

/*==================[internal data definition]===============================*/

/** Handle de la tarea de procesamiento ADC */
TaskHandle_t adc_task_handle = NULL;

/** Handle de la tarea de UMBRAL (visual) */
TaskHandle_t umbral_task_handle = NULL;

// CAMBIO: Un solo Handle para la tarea de sonido
/** Handle de la tarea de reproducción de sonido */
TaskHandle_t  playSound_task_handle = NULL;

/** Variable para almacenar la conversión ADC del PAD A (12 bits) */
static uint16_t valor_adc_A = 0;

/** Variable para almacenar la conversión ADC del PAD B (12 bits) */
static uint16_t valor_adc_B = 0;

/** Variable para almacenar los milivots registrados PAD A */
static uint32_t milliv_A = 0;

/** Variable para almacenar los milivots registrados PAD B */
static uint32_t milliv_B = 0;

/*==================[internal functions declaration]=========================*/
/**
 * @brief Callback del timer A - dispara conversión ADC cada 50μs (20kHz)
 */
void TimerAdcCallback(void *param);

/**
 * @brief Tarea que procesa y transmite datos del ADC
 */
static void AdcTask(void *pvParameters);


/**
 * @brief Tarea que enciende el LED momentáneamente al detectar un golpe
 */
static void UmbralTask(void *pvParameters);


// CAMBIO: Declaración de la nueva tarea de sonido unificada
/**
 * @brief Tarea que reproduce un sonido (Snare o HiHat) basado en una notificación
 */
static void PlaySoundTask(void *pvParameters);

/*==================[external functions definition]==========================*/

void TimerAdcCallback(void *param) {
    // Notifica a la tarea AdcTask para que se ejecute
    vTaskNotifyGiveFromISR(adc_task_handle, NULL);
}

// CAMBIO: Tarea de sonido unificada
static void PlaySoundTask(void *pvParameters) {
    uint32_t soundToPlay;

    while (true) {
        // Espera permanentemente hasta recibir una notificación
        if (xTaskNotifyWait(0, 0xFFFFFFFF, &soundToPlay, portMAX_DELAY) == pdTRUE) {
            
            // Revisa qué sonido debe reproducir según el valor de la notificación
            if (soundToPlay == PLAY_SNARE) {
                
                // CAMBIO: Corregido el bug de 'sizeof'. 
                // Usamos el tamaño real del array (definido en drum_samples.c)
                for (int i = 0; i < snare_drum_size; i++) {
                    AnalogOutputWrite(snare_drum_sample[i]);
                    // Espera el tiempo de muestreo del audio
                    vTaskDelay(pdMS_TO_TICKS(1000 / SAMPLE_RATE)); 
                }

            } else if (soundToPlay == PLAY_HIHAT) {
                
                // CAMBIO: Corregido el bug de 'sizeof'.
                for (int i = 0; i < hi_hat_size; i++) {
                    AnalogOutputWrite(hi_hat_sample[i]);
                    vTaskDelay(pdMS_TO_TICKS(1000 / SAMPLE_RATE));
                }
            }
            // Aseguramos que el DAC quede en silencio (valor medio)
            AnalogOutputWrite(512); 
        }
    }
}


static void AdcTask(void *pvParameters) {
    
    // CAMBIO: Variables para gestionar el cooldown (antirrebote) de los pads
    uint32_t last_hit_time_A = 0;
    uint32_t last_hit_time_B = 0;
    uint32_t current_time = 0;

    while (1) {
        // Espera la notificación del Timer (cada 50us)
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // Lee ambos conversores ADC 
        AnalogInputReadSingle(ADC_CHANNEL_A, &valor_adc_A);
        AnalogInputReadSingle(ADC_CHANNEL_B, &valor_adc_B); 
        
        // Convierte a milivoltios
        milliv_A = (uint32_t)((valor_adc_A * 3300UL) / 4095UL);
        milliv_B = (uint32_t)((valor_adc_B * 3300UL) / 4095UL); 
       
        // Envía los datos por UART
        // (Ver "Advertencia de Rendimiento" al final de esta respuesta)
        char buffer[32];
        sprintf(buffer, "A:%lu,B:%lu\r\n", (unsigned long)milliv_A, (unsigned long)milliv_B);
        UartSendString(UART_PC, buffer);
        
        // --- Lógica de Detección de Golpes ---
        
        // CAMBIO: Obtenemos el tiempo actual
        current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // Comprueba PAD A
        if ((milliv_A > ADC_THRESHOLD_MV_MINIMUM) && (current_time - last_hit_time_A > HIT_COOLDOWN_MS)) {
            last_hit_time_A = current_time; // Registra el tiempo del golpe
            
            // Notifica a la tarea del LED
            vTaskNotifyGive(umbral_task_handle); 
            
            // CAMBIO: Notifica a la tarea de sonido, enviando el valor PLAY_SNARE
            xTaskNotify(playSound_task_handle, PLAY_SNARE, eSetValueWithOverwrite);
        } 
        
        // Comprueba PAD B
        if ((milliv_B > ADC_THRESHOLD_MV_MINIMUM) && (current_time - last_hit_time_B > HIT_COOLDOWN_MS)) {
            last_hit_time_B = current_time; // Registra el tiempo del golpe

            vTaskNotifyGive(umbral_task_handle);

            // CAMBIO: Notifica a la tarea de sonido, enviando el valor PLAY_HIHAT
            xTaskNotify(playSound_task_handle, PLAY_HIHAT, eSetValueWithOverwrite);
        }
    }
}

/**
 * @brief Tarea que monitorea el valor del ADC y reproduce sonido (o activa pin)
 * cuando se supera el umbral configurado.
 */
static void UmbralTask(void *pvParameters) {
    // Esta tarea solo controla el LED como feedback visual
    while(true){
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Espera un golpe
        NeoPixelAllColor(NEOPIXEL_COLOR_RED);   
        vTaskDelay(pdMS_TO_TICKS(125)); // Mantiene el LED encendido 125ms
        NeoPixelAllOff(); 
    }
}

void app_main(void) {

    analog_input_config_t adc_config_A = {
        .input = ADC_CHANNEL_A,   // Canal CH1 para PAD A
        .mode = ADC_SINGLE,
        .func_p = NULL,         
        .param_p = NULL,
        .sample_frec = 0        
    };
    
    analog_input_config_t adc_config_B = { 
        .input = ADC_CHANNEL_B,   // Canal CH0 para PAD B
        .mode = ADC_SINGLE,
        .func_p = NULL,         
        .param_p = NULL,
        .sample_frec = 0        
    };

    // Inicialización UART 
   serial_config_t uart_config = {
        .port = UART_PC,
        .baud_rate = UART_BAUD_RATE,
        .func_p = NULL,         
        .param_p = NULL
    };
    
    // Configuración timer A para muestreo ADC (20kHz)
    timer_config_t timer_adc_config = {
        .timer = TIMER_A,
        .period = TIMER_ADC_PERIOD_US,  // Cada 50μs = 20kHz
        .func_p = TimerAdcCallback,     // Callback para lectura ADC
        .param_p = NULL
    };

    
    neopixel_color_t LED_UNICO;
    TimerInit(&timer_adc_config);
    AnalogInputInit(&adc_config_A);
    AnalogInputInit(&adc_config_B); 
    AnalogOutputInit();
    UartInit(&uart_config);
    NeoPixelInit(BUILT_IN_RGB_LED_PIN ,  BUILT_IN_RGB_LED_LENGTH ,  &LED_UNICO );
    
    
    // Crear tareas
    xTaskCreate(AdcTask, "AdcTask", 4096, NULL, 5, &adc_task_handle);
    xTaskCreate(UmbralTask, "UmbralTask", 4096, NULL, 5, &umbral_task_handle);
    xTaskCreate(PlaySoundTask, "PlaySoundTask", 4096, NULL, 5, &playSound_task_handle);

    // Iniciar el timer que dispara todo el proceso
    TimerStart(TIMER_A); 
    
}
/*==================[end of file]============================================*/