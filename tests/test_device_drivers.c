#include "main.h"
#include "tim.h"
#include "hc_sr04.h"
#include "zs040.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

FakeUart fake_uart;
FakeRcc fake_rcc = { .CFGR = RCC_CFGR_PPRE1 };
TIM_HandleTypeDef htim5;
uint32_t fake_tick, fake_us, fake_irq, fake_pending, fake_echo, fake_trig, fake_last_trig;

static void Edge(uint16_t pin, uint32_t timestamp, uint32_t high)
{
    fake_us = timestamp;
    if (high) fake_echo |= pin;
    else fake_echo &= ~(uint32_t)pin;
    fake_pending |= pin;
    HAL_GPIO_EXTI_IRQHandler(pin);
    assert(!(fake_pending & pin));
}

static void Trigger(uint32_t ms, uint32_t us, uint16_t expected_pin)
{
    fake_tick = ms;
    fake_us = us;
    fake_last_trig = 0;
    HCSR04_Update();
    assert(fake_last_trig == expected_pin && !fake_trig && !fake_irq);
}

static void Rx(uint8_t byte, uint32_t ms, uint32_t error)
{
    fake_uart.DR = byte;
    fake_uart.SR = USART_SR_RXNE | error;
    fake_tick = ms;
    ZS040_RxIRQ();
    fake_uart.SR = 0; /* Host facade cannot emulate read-to-clear registers. */
}

static void SendAT(const char *expected)
{
    char sent[32] = {0};
    unsigned count = 0;
    for (unsigned i = 0; i < sizeof(sent); ++i)
    {
        fake_uart.SR = USART_SR_TXE | USART_SR_TC;
        fake_uart.DR = 0;
        ZS040_Update();
        ++fake_tick;
        if (!fake_uart.DR) break;
        assert(count + 1U < sizeof(sent));
        sent[count++] = (char)fake_uart.DR;
    }
    assert(strcmp(sent, expected) == 0);
}

static void ReplyAT(const char *reply)
{
    while (*reply) Rx((uint8_t)*reply++, ++fake_tick, 0);
    ZS040_Update();
    fake_tick += 30;
    ZS040_Update();
}

static void ConfigStart(void)
{
    fake_tick = 0;
    ZS040_Init();
    assert(zs040_config_status == ZS040_CONFIG_WAITING);
    assert(!ZS040_Write("x", 1));
    Rx('F', 100, 0);
    uint8_t byte;
    uint32_t timestamp;
    assert(!ZS040_Read(&byte, &timestamp)); /* No driving before setup finishes. */
    fake_tick = ZS040_AT_BOOT_MS;
    SendAT("AT");
}

static void ConfigFinish(ZS040ConfigStatus expected)
{
    fake_tick += 100;
    ZS040_Update();
    assert(zs040_config_status == expected);
    uint8_t byte;
    uint32_t timestamp;
    assert(!ZS040_Read(&byte, &timestamp)); /* AT replies never reach the car. */
}

static void TestUltrasonic(void)
{
    float cm;
    HCSR04_Init();
    assert(HCSR04_GetReading(HCSR04_FRONT).status == HCSR04_WAITING);
    assert(!HCSR04_Read(HCSR04_FRONT, &cm) && cm == 0.0f);
    assert(!HCSR04_Read(HCSR04_SENSOR_COUNT, &cm));

    Trigger(0, 0, US_FRONT_TRIG_Pin);
    Edge(US_LEFT_ECHO_Pin, 50, 1);
    Edge(US_LEFT_ECHO_Pin, 80, 0); /* Ignore the inactive sensor. */
    Edge(US_FRONT_ECHO_Pin, 100, 1);
    Edge(US_FRONT_ECHO_Pin, 5900, 0);
    assert(HCSR04_Read(HCSR04_FRONT, &cm) && fabsf(cm - 100.0f) < 0.001f);
    assert(!HCSR04_Read(HCSR04_LEFT, &cm));
    Trigger(69, 69000, 0);

    Trigger(70, 70000, US_LEFT_TRIG_Pin);
    Edge(US_LEFT_ECHO_Pin, 70100, 1);
    Edge(US_LEFT_ECHO_Pin, 73000, 0);
    assert(HCSR04_Read(HCSR04_LEFT, &cm) && fabsf(cm - 50.0f) < 0.001f);

    Trigger(140, 140000, US_RIGHT_TRIG_Pin);
    Edge(US_RIGHT_ECHO_Pin, 140100, 1);
    Edge(US_RIGHT_ECHO_Pin, 151700, 0);
    assert(HCSR04_Read(HCSR04_RIGHT, &cm) && fabsf(cm - 200.0f) < 0.001f);

    Trigger(210, 210000, US_FRONT_TRIG_Pin);
    Trigger(255, 255000, 0); /* Missing rising edge times out. */
    assert(!HCSR04_Read(HCSR04_FRONT, &cm));
    assert(HCSR04_GetReading(HCSR04_FRONT).status == HCSR04_NO_ECHO);
    assert(HCSR04_Read(HCSR04_LEFT, &cm));
    assert(HCSR04_Read(HCSR04_RIGHT, &cm));
    fake_echo |= US_LEFT_ECHO_Pin;
    Trigger(280, 280000, 0); /* Stuck high skips this sensor. */
    assert(!HCSR04_Read(HCSR04_LEFT, &cm));
    assert(HCSR04_GetReading(HCSR04_LEFT).status == HCSR04_ECHO_HIGH);
    fake_echo &= ~US_LEFT_ECHO_Pin;

    Trigger(350, 350000, US_RIGHT_TRIG_Pin);
    Edge(US_RIGHT_ECHO_Pin, 350100, 1);
    Edge(US_RIGHT_ECHO_Pin, 350110, 0); /* Impossible short pulse. */
    assert(!HCSR04_Read(HCSR04_RIGHT, &cm));
    assert(HCSR04_GetReading(HCSR04_RIGHT).status == HCSR04_SHORT_PULSE);

    Trigger(420, 420000, US_FRONT_TRIG_Pin);
    Edge(US_FRONT_ECHO_Pin, 420100, 1);
    fake_tick = 458;
    Edge(US_FRONT_ECHO_Pin, 458100, 0); /* Completed far pulse. */
    assert(!HCSR04_Read(HCSR04_FRONT, &cm));
    HCSR04_Reading reading = HCSR04_GetReading(HCSR04_FRONT);
    assert(reading.status == HCSR04_FAR && reading.cm == 400.0f && reading.sampled_ms == 458);

    Trigger(490, UINT32_MAX - 200U, US_LEFT_TRIG_Pin);
    Edge(US_LEFT_ECHO_Pin, UINT32_MAX - 100U, 1);
    Edge(US_LEFT_ECHO_Pin, 5699, 0); /* 5800 us across timer wrap. */
    assert(HCSR04_Read(HCSR04_LEFT, &cm) && fabsf(cm - 100.0f) < 0.001f);

    Trigger(560, 560000, US_RIGHT_TRIG_Pin);
    Edge(US_RIGHT_ECHO_Pin, 560100, 1);
    Trigger(605, 605000, 0); /* Rising edge alone is not a clearance reading. */
    assert(HCSR04_GetReading(HCSR04_RIGHT).status == HCSR04_ECHO_HIGH);
    Edge(US_RIGHT_ECHO_Pin, 606000, 0);
    fake_tick = 791;
    assert(!HCSR04_Read(HCSR04_LEFT, &cm) && cm == 0.0f);
    assert(HCSR04_GetReading(HCSR04_LEFT).status == HCSR04_STALE);
}

static void TestTelemetryTx(void)
{
    const char expected[] = "@US,D,61,N,0,F,400,71,71\n";
    char reply[sizeof(expected)];
    strcpy(reply, expected);
    assert(ZS040_Write(reply, sizeof(reply) - 1));
    memset(reply, 0, sizeof(reply)); /* The driver owns its queued copy. */
    assert(!ZS040_Write("x", 1));
    fake_uart.SR = fake_uart.DR = 0;
    ZS040_Update();
    assert(fake_uart.DR == 0); /* The loop stays nonblocking with TXE low. */
    for (unsigned i = 0; i < sizeof(expected) - 1U; ++i)
    {
        fake_uart.SR = USART_SR_TXE;
        ZS040_Update();
        assert(fake_uart.DR == (uint8_t)expected[i]);
    }
    assert(ZS040_Write("x", 1));
}

int main(void)
{
    TestUltrasonic();

    ConfigStart();
    assert(fake_uart.BRR == 10417U);
    ReplyAT("OK");
    SendAT("AT+NAMEMoonCar");
    ReplyAT("OKsetname");
    SendAT("AT+PIN0000");
    ReplyAT("OKsetPIN");
    ConfigFinish(ZS040_CONFIG_OK);
    TestTelemetryTx();
    uint8_t byte;
    uint32_t timestamp;
    assert(!ZS040_Read(&byte, &timestamp));
    Rx('F', 1000, 0);
    Rx('S', 1001, 0);
    assert(ZS040_Read(&byte, &timestamp) && byte == 'F' && timestamp == 1000);
    assert(ZS040_Read(&byte, &timestamp) && byte == 'S' && timestamp == 1001);
    assert(!ZS040_Read(&byte, &timestamp));
    Rx('F', 1002, 0);
    Rx('B', 1003, USART_SR_ORE);
    assert(!ZS040_Read(&byte, &timestamp));
    assert(ZS040_TakeFault());
    assert(!ZS040_TakeFault());
    assert(!ZS040_Read(&byte, &timestamp));
    for (unsigned i = 0; i < 64; ++i) Rx('F', i, 0);
    assert(!ZS040_Read(&byte, &timestamp) && ZS040_TakeFault());
    Rx('S', 2000, 0);
    assert(ZS040_Read(&byte, &timestamp) && byte == 'S');
    fake_irq = 1;
    assert(!ZS040_Read(&byte, &timestamp) && fake_irq == 1);
    fake_irq = 0;
    ConfigStart();
    fake_tick += ZS040_AT_TIMEOUT_MS;
    ZS040_Update();
    ConfigFinish(ZS040_CONFIG_NO_AT);
    ConfigStart();
    ReplyAT("OK");
    SendAT("AT+NAMEMoonCar");
    ReplyAT("ERROR");
    fake_tick += ZS040_AT_TIMEOUT_MS;
    ZS040_Update();
    ConfigFinish(ZS040_CONFIG_NAME_FAILED);
    ConfigStart();
    ReplyAT("Ok");
    SendAT("AT+NAMEMoonCar");
    ReplyAT("OKname");
    SendAT("AT+PIN0000");
    fake_tick += ZS040_AT_TIMEOUT_MS;
    ZS040_Update();
    ConfigFinish(ZS040_CONFIG_PIN_FAILED);
    ConfigStart();
    Rx('X', fake_tick, USART_SR_FE);
    ZS040_Update();
    ConfigFinish(ZS040_CONFIG_UART_ERROR);
    puts("device driver simulation tests passed");
}
