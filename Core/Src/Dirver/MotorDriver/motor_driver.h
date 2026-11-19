#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H

#include "main.h"

/* Direction pins; initialized by MX_GPIO_Init(). */
#define MOTOR_A_IN1_GPIO_PORT GPIOC
#define MOTOR_A_IN1_PIN GPIO_PIN_0
#define MOTOR_A_IN1_GPIO_CLK_ENABLE() __HAL_RCC_GPIOC_CLK_ENABLE()
#define MOTOR_A_IN2_GPIO_PORT GPIOC
#define MOTOR_A_IN2_PIN GPIO_PIN_1
#define MOTOR_A_IN2_GPIO_CLK_ENABLE() __HAL_RCC_GPIOC_CLK_ENABLE()
#define MOTOR_B_IN1_GPIO_PORT GPIOC
#define MOTOR_B_IN1_PIN GPIO_PIN_2
#define MOTOR_B_IN1_GPIO_CLK_ENABLE() __HAL_RCC_GPIOC_CLK_ENABLE()
#define MOTOR_B_IN2_GPIO_PORT GPIOC
#define MOTOR_B_IN2_PIN GPIO_PIN_3
#define MOTOR_B_IN2_GPIO_CLK_ENABLE() __HAL_RCC_GPIOC_CLK_ENABLE()

/* PWM pins must match TIM2 channels; initialized by MX_TIM2_Init(). */
#define MOTOR_A_PWM_GPIO_PORT GPIOA
#define MOTOR_A_PWM_PIN GPIO_PIN_0
#define MOTOR_A_PWM_GPIO_AF GPIO_AF1_TIM2
#define MOTOR_A_PWM_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()
#define MOTOR_A_PWM_CHANNEL TIM_CHANNEL_1
#define MOTOR_B_PWM_GPIO_PORT GPIOA
#define MOTOR_B_PWM_PIN GPIO_PIN_1
#define MOTOR_B_PWM_GPIO_AF GPIO_AF1_TIM2
#define MOTOR_B_PWM_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()
#define MOTOR_B_PWM_CHANNEL TIM_CHANNEL_2

/* Call after MX_GPIO_Init() and MX_TIM2_Init(). */
void MotorDriver_Init(void);
/* Signed PWM percentage, clamped to -100..100. */
void MotorA(int speed);
void MotorB(int speed);
/* Periodic distance estimate using the current motor commands. */
void MotorDriver_UpdateDistance(void);

#endif
