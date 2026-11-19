#include "zs040.h"
#include "main.h"
#include <string.h>

#define RX_SIZE 64U
static volatile uint8_t data[RX_SIZE];
static volatile uint32_t ticks[RX_SIZE];
static volatile uint32_t head, tail;
static volatile uint8_t fault;
volatile ZS040ConfigStatus zs040_config_status;
static ZS040ConfigStatus config_result;
static unsigned config_step, tx_index, response_length;
static uint32_t step_ms, last_rx_ms;
static char response[32];
static const char *const commands[] = {
    "AT", "AT+NAME" ZS040_DEVICE_NAME, "AT+PIN" ZS040_PAIRING_PIN
};
static uint8_t ReadRx(uint8_t *byte, uint32_t *received_ms);

void ZS040_Init(void)
{
    ZS040_TX_GPIO_CLK_ENABLE();
    ZS040_RX_GPIO_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_USART1_FORCE_RESET();
    __HAL_RCC_USART1_RELEASE_RESET();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = ZS040_TX_PIN;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = ZS040_TX_GPIO_AF;
    HAL_GPIO_Init(ZS040_TX_GPIO_PORT, &gpio);
    gpio.Pin = ZS040_RX_PIN;
    gpio.Alternate = ZS040_RX_GPIO_AF;
    HAL_GPIO_Init(ZS040_RX_GPIO_PORT, &gpio);
    /* Oversampling 16: BRR encodes the rounded PCLK / baud divisor. */
    USART1->BRR = (HAL_RCC_GetPCLK2Freq() + ZS040_BAUD_RATE / 2U) / ZS040_BAUD_RATE;
    USART1->CR2 = 0;
    USART1->CR3 = USART_CR3_EIE;
    head = tail = fault = 0;
    zs040_config_status = ZS040_CONFIG_WAITING;
    config_result = ZS040_CONFIG_OK;
    config_step = tx_index = response_length = 0;
    step_ms = HAL_GetTick();
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE |
                  USART_CR1_RXNEIE | USART_CR1_PEIE;
    HAL_NVIC_SetPriority(USART1_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

void ZS040_RxIRQ(void)
{
    uint32_t status = USART1->SR;
    if (!(status & (USART_SR_RXNE | USART_SR_ORE | USART_SR_NE |
                    USART_SR_FE | USART_SR_PE))) return;
    /* Reading SR then DR clears RXNE and line/overrun error flags. */
    uint8_t byte = (uint8_t)USART1->DR;
    if (status & (USART_SR_ORE | USART_SR_NE | USART_SR_FE | USART_SR_PE))
    {
        fault = 1;
        return;
    }
    if (fault) return;
    uint32_t next = (head + 1U) % RX_SIZE;
    if (next == tail) { fault = 1; return; }
    data[head] = byte;
    ticks[head] = HAL_GetTick();
    __DMB();
    head = next;
}

static uint8_t ReadRx(uint8_t *byte, uint32_t *received_ms)
{
    uint32_t irq = __get_PRIMASK();
    __disable_irq();
    uint8_t ready = !fault && tail != head;
    if (ready)
    {
        *byte = data[tail];
        *received_ms = ticks[tail];
        tail = (tail + 1U) % RX_SIZE;
    }
    __set_PRIMASK(irq);
    return ready;
}

uint8_t ZS040_TakeFault(void)
{
    uint32_t irq = __get_PRIMASK();
    __disable_irq();
    uint8_t result = fault;
    if (fault) { tail = head; fault = 0; }
    __set_PRIMASK(irq);
    return result;
}

uint8_t ZS040_Read(uint8_t *byte, uint32_t *received_ms)
{
    if (zs040_config_status == ZS040_CONFIG_WAITING) return 0;
    return ReadRx(byte, received_ms);
}

static void ClearRx(void)
{
    uint32_t irq = __get_PRIMASK();
    __disable_irq();
    tail = head;
    fault = 0;
    __set_PRIMASK(irq);
}

static void FinishConfig(ZS040ConfigStatus result, uint32_t now)
{
    config_result = result;
    config_step = 7; /* Drain late AT responses before enabling car commands. */
    step_ms = now;
    ClearRx();
}

static uint8_t ResponseMatches(unsigned command)
{
    /* Firmware variants differ in capitalization and acknowledgment suffix. */
    if (command == 0) return strcmp(response, "OK") == 0;
    if (command == 1)
        return strcmp(response, "OKSETNAME") == 0 || strcmp(response, "OKNAME") == 0;
    return strcmp(response, "OKSETPIN") == 0 ||
           strcmp(response, "OK" ZS040_PAIRING_PIN) == 0;
}

void ZS040_Update(void)
{
    if (zs040_config_status != ZS040_CONFIG_WAITING) return;
    uint32_t now = HAL_GetTick();
    if (config_step == 7)
    {
        ClearRx();
        if ((uint32_t)(now - step_ms) >= 100U)
            zs040_config_status = config_result;
        return;
    }
    if (ZS040_TakeFault())
    {
        FinishConfig(ZS040_CONFIG_UART_ERROR, now);
        return;
    }
    if (config_step == 0)
    {
        if ((uint32_t)(now - step_ms) < ZS040_AT_BOOT_MS) return;
        ClearRx();
        config_step = 1;
        step_ms = now;
    }
    unsigned command = (config_step - 1U) / 2U;
    if (config_step & 1U)
    {
        /* One byte per loop; no blocking waits during initialization. */
        if (commands[command][tx_index])
        {
            if (USART1->SR & USART_SR_TXE)
                USART1->DR = (uint8_t)commands[command][tx_index++];
        }
        else if (USART1->SR & USART_SR_TC)
        {
            ++config_step;
            step_ms = now;
            response_length = 0;
            response[0] = '\0';
            return;
        }
    }
    else
    {
        uint8_t byte;
        uint32_t rx_ms;
        for (unsigned i = 0; i < RX_SIZE && ReadRx(&byte, &rx_ms); ++i)
        {
            if (response_length + 1U >= sizeof(response))
            {
                FinishConfig(ZS040_CONFIG_UART_ERROR, now);
                return;
            }
            if (byte >= 'a' && byte <= 'z') byte -= 'a' - 'A';
            if (byte != '\r' && byte != '\n')
                response[response_length++] = (char)byte;
            response[response_length] = '\0';
            last_rx_ms = rx_ms;
        }
        /* Wait for the full response, rather than accepting an OK prefix. */
        if (response_length && (uint32_t)(now - last_rx_ms) >= 30U &&
            ResponseMatches(command))
        {
            if (command == 2) FinishConfig(ZS040_CONFIG_OK, now);
            else
            {
                ++config_step;
                tx_index = 0;
                step_ms = now;
                ClearRx();
            }
            return;
        }
    }
    if ((uint32_t)(now - step_ms) >= ZS040_AT_TIMEOUT_MS)
    {
        static const ZS040ConfigStatus failures[] = {
            ZS040_CONFIG_NO_AT, ZS040_CONFIG_NAME_FAILED, ZS040_CONFIG_PIN_FAILED
        };
        FinishConfig(failures[command], now);
    }
}
