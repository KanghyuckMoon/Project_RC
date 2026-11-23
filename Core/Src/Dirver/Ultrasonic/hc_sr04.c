#include "hc_sr04.h"
#include "tim.h"

#define CYCLE_MS 70U
#define TIMEOUT_US 45000U
#define STALE_MS 300U
#define MIN_DISTANCE_PULSE_US 116U
#define MAX_DISTANCE_PULSE_US 23200U

static const struct
{
    GPIO_TypeDef *trig_port;
    uint16_t trig_pin;
    GPIO_TypeDef *echo_port;
    uint16_t echo_pin;
} pins[HCSR04_SENSOR_COUNT] = {
    [HCSR04_FRONT] = { US_FRONT_TRIG_GPIO_Port, US_FRONT_TRIG_Pin,
                      US_FRONT_ECHO_GPIO_Port, US_FRONT_ECHO_Pin },
    [HCSR04_LEFT] = { US_LEFT_TRIG_GPIO_Port, US_LEFT_TRIG_Pin,
                     US_LEFT_ECHO_GPIO_Port, US_LEFT_ECHO_Pin },
    [HCSR04_RIGHT] = { US_RIGHT_TRIG_GPIO_Port, US_RIGHT_TRIG_Pin,
                      US_RIGHT_ECHO_GPIO_Port, US_RIGHT_ECHO_Pin },
};

typedef struct
{
    uint32_t pulse_us;
    uint32_t sample_ms;
    HCSR04_Status status;
} EchoSample;

static volatile EchoSample samples[HCSR04_SENSOR_COUNT];
static volatile enum { IDLE, WAIT_RISE, WAIT_FALL } phase;
static volatile HCSR04_Sensor active_sensor;
static volatile uint32_t trigger_us, rise_us;
static HCSR04_Sensor next_sensor;
static uint32_t last_trigger_ms;

static uint32_t Lock(void)
{
    uint32_t state = __get_PRIMASK();
    __disable_irq();
    return state;
}

static void FinishSample(HCSR04_Status status, uint32_t width, uint32_t now_ms)
{
    samples[active_sensor] = (EchoSample){width, now_ms, status};
    phase = IDLE;
}

void HCSR04_Init(void)
{
    phase = IDLE;
    for (unsigned i = 0; i < HCSR04_SENSOR_COUNT; ++i)
        samples[i] = (EchoSample){.status = HCSR04_WAITING};

    next_sensor = HCSR04_FRONT;
    last_trigger_ms = HAL_GetTick() - CYCLE_MS;
    if (HAL_TIM_Base_Start(&htim5) != HAL_OK) Error_Handler();
}

void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    HCSR04_Sensor sensor = active_sensor;
    if (phase == IDLE || pin != pins[sensor].echo_pin) return;

    uint32_t now = __HAL_TIM_GET_COUNTER(&htim5);
    if (now - trigger_us >= TIMEOUT_US)
    {
        FinishSample(phase == WAIT_RISE ? HCSR04_NO_ECHO : HCSR04_ECHO_HIGH, 0, HAL_GetTick());
        return;
    }

    if (HAL_GPIO_ReadPin(pins[sensor].echo_port, pin) == GPIO_PIN_SET)
    {
        if (phase == WAIT_RISE)
        {
            rise_us = now;
            phase = WAIT_FALL;
        }
    }
    else if (phase == WAIT_FALL)
    {
        uint32_t width = now - rise_us;
        HCSR04_Status status = HCSR04_DISTANCE;
        if (width < MIN_DISTANCE_PULSE_US) status = HCSR04_SHORT_PULSE;
        else if (width > MAX_DISTANCE_PULSE_US) status = HCSR04_FAR;
        FinishSample(status, width, HAL_GetTick());
    }
}

void HCSR04_Update(void)
{
    uint32_t now_ms = HAL_GetTick();
    uint32_t irq = Lock();
    uint32_t now_us = __HAL_TIM_GET_COUNTER(&htim5);
    if (phase != IDLE && now_us - trigger_us >= TIMEOUT_US)
    {
        FinishSample(phase == WAIT_RISE ? HCSR04_NO_ECHO : HCSR04_ECHO_HIGH, 0, now_ms);
    }
    if (phase != IDLE || now_ms - last_trigger_ms < CYCLE_MS)
    {
        __set_PRIMASK(irq);
        return;
    }

    /* Space the pulses across all sensors to limit acoustic crosstalk. */
    HCSR04_Sensor sensor = next_sensor;
    next_sensor = (sensor + 1U) % HCSR04_SENSOR_COUNT;
    last_trigger_ms = now_ms;
    if (HAL_GPIO_ReadPin(pins[sensor].echo_port, pins[sensor].echo_pin) == GPIO_PIN_SET)
    {
        samples[sensor] = (EchoSample){.sample_ms = now_ms, .status = HCSR04_ECHO_HIGH};
        __set_PRIMASK(irq);
        return;
    }
    __HAL_GPIO_EXTI_CLEAR_IT(pins[sensor].echo_pin);
    active_sensor = sensor;
    trigger_us = now_us;
    phase = WAIT_RISE;
    __set_PRIMASK(irq);

    HAL_GPIO_WritePin(pins[sensor].trig_port, pins[sensor].trig_pin, GPIO_PIN_SET);
    uint32_t start = __HAL_TIM_GET_COUNTER(&htim5);
    while (__HAL_TIM_GET_COUNTER(&htim5) - start < 12U) { }
    HAL_GPIO_WritePin(pins[sensor].trig_port, pins[sensor].trig_pin, GPIO_PIN_RESET);
}

HCSR04_Reading HCSR04_GetReading(HCSR04_Sensor sensor)
{
    if ((unsigned)sensor >= HCSR04_SENSOR_COUNT)
        return (HCSR04_Reading){.status = HCSR04_WAITING};

    uint32_t irq = Lock();
    EchoSample sample = samples[sensor];
    __set_PRIMASK(irq);
    if (sample.status != HCSR04_WAITING && HAL_GetTick() - sample.sample_ms > STALE_MS)
        sample.status = HCSR04_STALE;

    float cm = 0.0f;
    if (sample.status == HCSR04_DISTANCE) cm = (float)sample.pulse_us / 58.0f;
    else if (sample.status == HCSR04_FAR) cm = 400.0f;
    return (HCSR04_Reading){sample.status, cm, sample.sample_ms};
}

uint8_t HCSR04_Read(HCSR04_Sensor sensor, float *distance_cm)
{
    HCSR04_Reading reading = HCSR04_GetReading(sensor);
    uint8_t valid = reading.status == HCSR04_DISTANCE;
    if (distance_cm) *distance_cm = valid ? reading.cm : 0.0f;
    return valid;
}
