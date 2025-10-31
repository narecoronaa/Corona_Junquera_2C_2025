/*! @mainpage Template
 *
/*! @mainpage Template
 *
 * @section genDesc General Description
 *
 * This section describes how the program works.
 *
 * <a href="https://drive.google.com/...">Operation Example</a>
 *
 * @section hardConn Hardware Connection
 *
 * |    Peripheral  |   ESP32   	|
 * |:--------------:|:--------------|
 * | 	PIN_X	 	| 	GPIO_X		|
 *
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 28/10/2025 | Document creation		                         |
 *
 * @author Santiago Ulises Junquera (santiago.junquera@ingenieria.uner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include "gpio_mcu.h"
#include "timer_mcu.h"
#include "uart_mcu.h"
#include "analog_io_mcu.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "buzzer.h"
#include "buzzer_melodies.h"

/*==================[macros and definitions]=================================*/

#define TIMER_PERIOD 2000 // 2 milisegundos
uint16_t ValorP1;
uint16_t ValorP2;
//ENTRADA DE CORRIENTE EN PRESÓN MAXIMA -->2,2 uA

/*==================[internal data definition]===============================*/

TaskHandle_t ObtenerGolpeP1_task_handle = NULL;
TaskHandle_t ObtenerGolpeP2_task_handle = NULL;

/*==================[internal functions declaration]=========================*/

void Timer_medir()
{
    vTaskNotifyGiveFromISR(ObtenerGolpeP1_task_handle, NULL);  
	vTaskNotifyGiveFromISR(ObtenerGolpeP2_task_handle, NULL);  
}

static void ObtenerGolpeP1( void *pvParameters )
{
    while (true)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        AnalogInputReadSingle(CH1,&ValorP1);
    }
}

static void ObtenerGolpeP2( void *pvParameters )
{
    while (true)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        AnalogInputReadSingle(CH1,&ValorP2);

    }
}
/*==================[external functions definition]==========================*/
void app_main(void){


 timer_config_t timer_p = {
        .timer = TIMER_A ,
        .period = TIMER_PERIOD, // 2 ms
        .func_p = Timer_medir,
        .param_p = NULL
    };

	analog_input_config_t analog_input1= {			
		. input = CH1 ,			/*!< Inputs: CH0, CH1, CH2, CH3 */
		. mode= ADC_SINGLE,	/*!< Mode: single read or continuous read */
		. func_p = ObtenerGolpeP1, 			/*!< Pointer to callback function for convertion end (only for continuous mode) */
		. param_p= NULL ,			/*!< Pointer to callback function parameters (only for continuous mode) */
		. sample_frec= 0/*!< Sample frequency min: 20kHz - max: 2MHz (only for continuous mode)  */
	};	

	analog_input_config_t analog_input2= {			
		. input = CH2 ,			/*!< Inputs: CH0, CH1, CH2, CH3 */
		. mode= ADC_SINGLE,	/*!< Mode: single read or continuous read */
		. func_p = ObtenerGolpeP2, 			/*!< Pointer to callback function for convertion end (only for continuous mode) */
		. param_p= NULL ,			/*!< Pointer to callback function parameters (only for continuous mode) */
		. sample_frec= 0/*!< Sample frequency min: 20kHz - max: 2MHz (only for continuous mode)  */
	};	

	xTaskCreate(&ObtenerGolpeP1, "ADC", 512, NULL, 5, &ObtenerGolpeP1_task_handle);	
	xTaskCreate(&ObtenerGolpeP2, "ADC", 512, NULL, 5, &ObtenerGolpeP2_task_handle);
	

    TimerInit(&timer_p);
	AnalogInputInit(&analog_input1);
	AnalogInputInit(&analog_input2);
	
    TimerStart(TIMER_A);

}
/*==================[end of file]============================================*/
/*==================[end of file]============================================*/