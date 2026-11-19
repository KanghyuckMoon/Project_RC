#ifndef CAR_APP_H
#define CAR_APP_H
#include "car_control.h"

/* Keep these pins in sync with CubeMX GPIO output configuration. */
#define CAR_RX_LED_GPIO_PORT GPIOA
#define CAR_RX_LED_PIN GPIO_PIN_5

/* Debugger watch: state, manual_motion, motor_a/b. */
extern CarControl car_control;
void CarApp_Init(void);
void CarApp_Update(void);
#endif
