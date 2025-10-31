/*! @mainpage Osciloscopio digital con ADC y transmisión UART
 *
 * @section genDesc Descripción General
 *
 * Esta aplicación digitaliza una señal analógica del canal CH1 del conversor AD
 * y la transmite por UART a un graficador de puerto serie de la PC.
 *
 * PARTE 1: Lectura de potenciómetro a 500Hz para visualización en Serial Oscilloscope
 * PARTE 2: Generación de señal ECG por DAC a 250Hz (cada 4ms) 
 *
 * @section hardConn Conexión de Hardware
 *
 * |    Peripheral  |   ESP32   	|
 * |:--------------:|:--------------|
 * | 	CH1 ADC	 	| 	GPIO_1		|
 * | 	UART_PC	 	| 	USB			|
 * | 	CH0 DAC	    | 	GPIO_25		|
 *
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 24/10/2025 | Implementación osciloscopio digital            |
 *
 * @author Corona Narella (narella.corona@ingenieria.uner.edu.ar)
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

/*==================[macros and definitions]=================================*/
/** Período del timer A para muestreo ADC: 20KHz = 50μs */
#define TIMER_ADC_PERIOD_US     50


/** Canal ADC a usar para potenciómetro */
#define ADC_CHANNEL             CH1

/** Velocidad UART para transmisión */
#define UART_BAUD_RATE          921600

/*==================[internal data definition]===============================*/
/** Handle de la tarea de procesamiento ADC */
TaskHandle_t adc_task_handle = NULL;

/** Variable para almacenar la conversión ADC del potenciómetro (12 bits) */
uint16_t valor_adc = 0;

/*==================[internal functions declaration]=========================*/
/**
 * @brief Callback del timer A - dispara conversión ADC cada 50μs (20kHz)
 */
void TimerAdcCallback(void *param);

/**
 * @brief Tarea que procesa y transmite datos del ADC
 */
static void AdcTask(void *pvParameters);

/*==================[external functions definition]==========================*/

void TimerAdcCallback(void *param) {
    vTaskNotifyGiveFromISR(adc_task_handle, NULL);
}

static void AdcTask(void *pvParameters) {
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // Lee el conversor ADC 
        AnalogInputReadSingle(ADC_CHANNEL, &valor_adc);
        
        // Mejor enviar como entero (mV) para reducir uso de stack y evitar float en sprintf
        uint32_t milliv = (uint32_t)((valor_adc * 3300UL) / 4095UL);
        char buffer[32];
        // Formato: "1234\r\n"
        sprintf(buffer, "%lu\r\n", (unsigned long)milliv);
        UartSendString(UART_PC, buffer);
    }
}

void app_main(void) {
    analog_input_config_t adc_config = {
        .input = ADC_CHANNEL,   // Canal CH1 para potenciómetro
        .mode = ADC_SINGLE,     // Conversión individual
        .func_p = NULL,         
        .param_p = NULL,
        .sample_frec = 0        
    };
    AnalogInputInit(&adc_config);


    // Inicialización UART 
    serial_config_t uart_config = {
        .port = UART_PC,
        .baud_rate = UART_BAUD_RATE,
        .func_p = NULL,         
        .param_p = NULL
    };
    UartInit(&uart_config);
    
    // Configuración timer A para muestreo ADC (20kHz)
    timer_config_t timer_adc_config = {
        .timer = TIMER_A,
        .period = TIMER_ADC_PERIOD_US,  // Cada 50μs = 20kHz
        .func_p = TimerAdcCallback,     // Callback para lectura ADC
        .param_p = NULL
    };
    TimerInit(&timer_adc_config);
    
    xTaskCreate(AdcTask, "AdcTask", 4096, NULL, 5, &adc_task_handle);
    
    TimerStart(TIMER_A); 
    
}

/*==================[end of file]============================================*/