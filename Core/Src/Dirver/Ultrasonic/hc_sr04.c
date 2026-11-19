#include "hc_sr04.h"
#include "main.h"

#define CYCLE_MS 70U
#define TIMEOUT_US 30000U
#define STALE_MS 250U

static TIM_HandleTypeDef timer;
static volatile uint8_t phase; /* 0 idle, 1 waiting rise, 2 waiting fall */
static volatile uint8_t valid;
static volatile uint32_t trigger_us, rise_us, pulse_us, sample_ms;
static uint32_t last_trigger_ms;

static uint32_t Lock(void)
{
    uint32_t state = __get_PRIMASK();
    __disable_irq();
    return state;
}

void HCSR04_Init(void)
{
    HCSR04_TRIG_GPIO_CLK_ENABLE();
    HCSR04_ECHO_GPIO_CLK_ENABLE();
    __HAL_RCC_TIM5_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    HAL_GPIO_WritePin(HCSR04_TRIG_GPIO_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);
    gpio.Pin = HCSR04_TRIG_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(HCSR04_TRIG_GPIO_PORT, &gpio);
    gpio.Pin = HCSR04_ECHO_PIN;
    gpio.Mode = GPIO_MODE_IT_RISING_FALLING;
    gpio.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(HCSR04_ECHO_GPIO_PORT, &gpio);

    uint32_t clock = HAL_RCC_GetPCLK1Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != 0U) clock *= 2U;
    timer.Instance = TIM5;
    timer.Init.Prescaler = clock / 1000000U - 1U;
    timer.Init.CounterMode = TIM_COUNTERMODE_UP;
    timer.Init.Period = UINT32_MAX;
    timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    if (HAL_TIM_Base_Init(&timer) != HAL_OK ||
        HAL_TIM_Base_Start(&timer) != HAL_OK) Error_Handler();
    phase = valid = 0;
    last_trigger_ms = HAL_GetTick() - CYCLE_MS;
    __HAL_GPIO_EXTI_CLEAR_IT(HCSR04_ECHO_PIN);
    HAL_NVIC_SetPriority(HCSR04_ECHO_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(HCSR04_ECHO_IRQn);
}

void HCSR04_EchoIRQ(void)
{
    if (__HAL_GPIO_EXTI_GET_IT(HCSR04_ECHO_PIN) == RESET) return;
    __HAL_GPIO_EXTI_CLEAR_IT(HCSR04_ECHO_PIN);
    uint32_t now = __HAL_TIM_GET_COUNTER(&timer);
    if (phase == 0) return;
    if ((uint32_t)(now - trigger_us) >= TIMEOUT_US)
    {
        phase = valid = 0;
        return;
    }
    if (HAL_GPIO_ReadPin(HCSR04_ECHO_GPIO_PORT, HCSR04_ECHO_PIN) == GPIO_PIN_SET)
    {
        if (phase == 1) { rise_us = now; phase = 2; }
    }
    else if (phase == 2)
    {
        pulse_us = now - rise_us;
        /* Datasheet: 2..400 cm, pulse width / 58 = cm. */
        valid = pulse_us >= 116U && pulse_us <= 23200U;
        sample_ms = HAL_GetTick();
        phase = 0;
    }
}

void HCSR04_Update(void)
{
    uint32_t now_ms = HAL_GetTick();
    uint32_t irq = Lock();
    uint32_t now_us = __HAL_TIM_GET_COUNTER(&timer);
    if (phase && (uint32_t)(now_us - trigger_us) >= TIMEOUT_US)
        phase = valid = 0;
    if (phase || (uint32_t)(now_ms - last_trigger_ms) < CYCLE_MS)
    {
        __set_PRIMASK(irq);
        return;
    }
    last_trigger_ms = now_ms;
    if (HAL_GPIO_ReadPin(HCSR04_ECHO_GPIO_PORT, HCSR04_ECHO_PIN) == GPIO_PIN_SET)
    {
        valid = 0; /* Stuck high must not look like clear space. */
        __set_PRIMASK(irq);
        return;
    }
    __HAL_GPIO_EXTI_CLEAR_IT(HCSR04_ECHO_PIN);
    trigger_us = now_us;
    phase = 1;
    __set_PRIMASK(irq);

    HAL_GPIO_WritePin(HCSR04_TRIG_GPIO_PORT, HCSR04_TRIG_PIN, GPIO_PIN_SET);
    uint32_t start = __HAL_TIM_GET_COUNTER(&timer);
    while ((uint32_t)(__HAL_TIM_GET_COUNTER(&timer) - start) < 12U) { }
    HAL_GPIO_WritePin(HCSR04_TRIG_GPIO_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);
}

uint8_t HCSR04_Read(float *distance_cm)
{
    uint32_t irq = Lock();
    uint8_t ok = valid;
    uint32_t width = pulse_us, timestamp = sample_ms;
    __set_PRIMASK(irq);
    if ((uint32_t)(HAL_GetTick() - timestamp) > STALE_MS) ok = 0;
    if (distance_cm) *distance_cm = ok ? (float)width / 58.0f : 0.0f;
    return ok;
}
