#ifndef HC_SR04_H
#define HC_SR04_H

#include <stdint.h>
#include "main.h"

/* Keep ECHO pin, IRQn and vector consistent when changing EXTI lines. */
#define HCSR04_TRIG_GPIO_PORT GPIOB
#define HCSR04_TRIG_PIN GPIO_PIN_0
#define HCSR04_TRIG_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()
#define HCSR04_ECHO_GPIO_PORT GPIOB
#define HCSR04_ECHO_PIN GPIO_PIN_1
#define HCSR04_ECHO_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()
#define HCSR04_ECHO_IRQn EXTI1_IRQn
#define HCSR04_ECHO_IRQ_HANDLER EXTI1_IRQHandler

/* Default wiring: PB0 TRIG, PB1 ECHO (through a 5 V -> 3.3 V divider).
 * Owns TIM5 (1 MHz, 32-bit) and EXTI1. Init after SystemClock_Config(). */
void HCSR04_Init(void);
void HCSR04_Update(void);
void HCSR04_EchoIRQ(void);

/* Returns 0 at startup, on timeout, invalid pulse, or stale data.
 * Must be called from the main loop, not an ISR. */
uint8_t HCSR04_Read(float *distance_cm);

#endif
