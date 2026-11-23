#include "car_app.h"
#include "hc_sr04.h"
#include "main.h"
#include "motor_driver.h"
#include "zs040.h"
#include <stdio.h>

CarControl car_control;

#define RX_LED_ON_MS 100U
#define RX_LED_OFF_MS 70U

static uint32_t rx_led_changed_ms;
static enum { RX_LED_IDLE, RX_LED_ON, RX_LED_GAP } rx_led_state;
static uint8_t rx_led_pending;
static uint8_t telemetry_requested;

static CarRange ToCarRange(HCSR04_Reading reading)
{
    return (CarRange){reading.cm, reading.sampled_ms, reading.status == HCSR04_NO_ECHO};
}

static void SendTelemetry(const HCSR04_Reading *readings)
{
    char reply[48];
    int length = snprintf(reply, sizeof(reply), "@US,%c,%u,%c,%u,%c,%u,%d,%d\n",
                          readings[HCSR04_FRONT].status, (unsigned)readings[HCSR04_FRONT].cm,
                          readings[HCSR04_LEFT].status, (unsigned)readings[HCSR04_LEFT].cm,
                          readings[HCSR04_RIGHT].status, (unsigned)readings[HCSR04_RIGHT].cm,
                          car_control.motor_a, car_control.motor_b);
    if (length > 0 && length < (int)sizeof(reply) && ZS040_Write(reply, (uint8_t)length))
        telemetry_requested = 0;
}

static void UpdateRxLed(uint32_t now, uint8_t data_received)
{
    if (data_received) rx_led_pending = 1;

    if (rx_led_state == RX_LED_ON && now - rx_led_changed_ms >= RX_LED_ON_MS)
    {
        HAL_GPIO_WritePin(CAR_RX_LED_GPIO_PORT, CAR_RX_LED_PIN, GPIO_PIN_RESET);
        rx_led_state = RX_LED_GAP;
        rx_led_changed_ms = now;
    }
    if (rx_led_state == RX_LED_GAP && now - rx_led_changed_ms >= RX_LED_OFF_MS)
        rx_led_state = RX_LED_IDLE;
    if (rx_led_pending && rx_led_state == RX_LED_IDLE)
    {
        rx_led_pending = 0;
        rx_led_state = RX_LED_ON;
        rx_led_changed_ms = now;
        HAL_GPIO_WritePin(CAR_RX_LED_GPIO_PORT, CAR_RX_LED_PIN, GPIO_PIN_SET);
    }
}

void CarApp_Init(void)
{
    rx_led_state = RX_LED_IDLE;
    rx_led_pending = 0;
    telemetry_requested = 0;
    HAL_GPIO_WritePin(CAR_RX_LED_GPIO_PORT, CAR_RX_LED_PIN, GPIO_PIN_RESET);
    MotorDriver_Init();
    HCSR04_Init();
    CarControl_Init(&car_control);
    ZS040_Init();
}

void CarApp_Update(void)
{
    HCSR04_Update();
    ZS040_Update();
    if (zs040_config_status == ZS040_CONFIG_WAITING)
    {
        CarControl_Stop(&car_control);
        MotorA(0);
        MotorB(0);
        return;
    }
    uint8_t byte;
    uint8_t data_received = 0;
    uint32_t rx_ms;
    /* Bounded work even if a transmitter continuously sends data. */
    for (unsigned i = 0; i < 64U && ZS040_Read(&byte, &rx_ms); ++i)
    {
        if (byte == '?')
        {
            telemetry_requested = 1;
            continue;
        }
        data_received = 1;
        if ((uint32_t)(HAL_GetTick() - rx_ms) < CAR_COMMAND_TIMEOUT_MS)
            CarControl_Command(&car_control, byte, rx_ms);
    }
    UpdateRxLed(HAL_GetTick(), data_received);
    if (ZS040_TakeFault()) CarControl_Stop(&car_control);

    CarAutoRanges ranges = {0};
    HCSR04_Reading readings[HCSR04_SENSOR_COUNT];
    if (car_control.state == CAR_AUTO_MOVING || telemetry_requested)
    {
        for (unsigned i = 0; i < HCSR04_SENSOR_COUNT; ++i)
            readings[i] = HCSR04_GetReading(i);
        ranges.front = ToCarRange(readings[HCSR04_FRONT]);
        ranges.left = ToCarRange(readings[HCSR04_LEFT]);
        ranges.right = ToCarRange(readings[HCSR04_RIGHT]);
    }
    CarControl_Update(&car_control, HAL_GetTick(), &ranges);
    MotorDriver_UpdateDistance();
    MotorA(car_control.motor_a);
    MotorB(car_control.motor_b);
    if (telemetry_requested) SendTelemetry(readings);
}
