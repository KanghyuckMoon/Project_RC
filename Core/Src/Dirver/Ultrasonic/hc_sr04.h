#ifndef HC_SR04_H
#define HC_SR04_H

#include <stdint.h>

typedef enum
{
    HCSR04_FRONT,
    HCSR04_LEFT,
    HCSR04_RIGHT,
    HCSR04_SENSOR_COUNT
} HCSR04_Sensor;

typedef enum
{
    HCSR04_WAITING = 'W',
    HCSR04_DISTANCE = 'D',
    HCSR04_FAR = 'F',
    HCSR04_NO_ECHO = 'N',
    HCSR04_ECHO_HIGH = 'H',
    HCSR04_SHORT_PULSE = 'P',
    HCSR04_STALE = 'S'
} HCSR04_Status;

typedef struct
{
    HCSR04_Status status;
    float cm;
    uint32_t sampled_ms;
} HCSR04_Reading;

/* Call after MX_GPIO_Init() and MX_TIM5_Init(). */
void HCSR04_Init(void);
void HCSR04_Update(void);

/* Returns 0 at startup, on timeout, invalid pulse, or stale data.
 * Must be called from the main loop, not an ISR. */
uint8_t HCSR04_Read(HCSR04_Sensor sensor, float *distance_cm);

/* Completed far pulses report 400 cm. Other statuses without a distance report zero. */
HCSR04_Reading HCSR04_GetReading(HCSR04_Sensor sensor);

#endif
