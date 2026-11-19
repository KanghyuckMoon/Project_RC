#include "main.h"
#include "hc_sr04.h"
#include "zs040.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

FakeUart fake_uart;
FakeRcc fake_rcc = { .CFGR = RCC_CFGR_PPRE1 };
uint32_t fake_tick, fake_us, fake_irq, fake_pending, fake_echo, fake_trig;

static void Edge(uint32_t timestamp, uint32_t high)
{
    fake_us = timestamp;
    fake_echo = high;
    fake_pending = 1;
    HCSR04_EchoIRQ();
    assert(!fake_pending);
}

static void Trigger(uint32_t ms, uint32_t us)
{
    fake_tick = ms;
    fake_us = us;
    HCSR04_Update();
    assert(!fake_trig && !fake_irq);
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

int main(void)
{
    float cm;
    HCSR04_Init();
    assert(!HCSR04_Read(&cm));
    Trigger(0, 0);
    Edge(100, 1);
    Edge(5900, 0);
    assert(HCSR04_Read(&cm) && fabsf(cm - 100.0f) < 0.001f);
    Trigger(70, 70000);
    fake_us = 100000;
    fake_tick = 100;
    HCSR04_Update(); /* Missing rising edge timeout. */
    assert(!HCSR04_Read(&cm));
    Trigger(140, 140000);
    Edge(140100, 1);
    fake_tick = 171;
    fake_us = 171000;
    HCSR04_Update(); /* Stuck high during a measurement. */
    assert(!HCSR04_Read(&cm));
    Trigger(210, 210000); /* Stuck high at next trigger. */
    assert(!HCSR04_Read(&cm));
    fake_echo = 0;
    Trigger(280, 280000);
    Edge(280100, 1);
    Edge(280110, 0); /* Impossible short pulse. */
    assert(!HCSR04_Read(&cm));
    Trigger(350, 350000);
    Edge(350100, 1);
    Edge(373301, 0); /* Beyond 400 cm. */
    assert(!HCSR04_Read(&cm));
    Trigger(420, UINT32_MAX - 200U);
    Edge(UINT32_MAX - 100U, 1);
    Edge(5699, 0); /* 5800 us across 32-bit timer wrap. */
    assert(HCSR04_Read(&cm) && fabsf(cm - 100.0f) < 0.001f);
    fake_tick = 671;
    assert(!HCSR04_Read(&cm) && cm == 0.0f);

    ConfigStart();
    assert(fake_uart.BRR == 10417U);
    ReplyAT("OK");
    SendAT("AT+NAMEMoonCar");
    ReplyAT("OKsetname");
    SendAT("AT+PIN0000");
    ReplyAT("OKsetPIN");
    ConfigFinish(ZS040_CONFIG_OK);
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
