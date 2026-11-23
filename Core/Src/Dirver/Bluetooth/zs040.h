#ifndef ZS040_H
#define ZS040_H
#include <stdint.h>
#include "main.h"

/* Both pins must support USART1 with the selected alternate function. */
#define ZS040_TX_GPIO_PORT GPIOA
#define ZS040_TX_PIN GPIO_PIN_9
#define ZS040_TX_GPIO_AF GPIO_AF7_USART1
#define ZS040_TX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()
#define ZS040_RX_GPIO_PORT GPIOA
#define ZS040_RX_PIN GPIO_PIN_10
#define ZS040_RX_GPIO_AF GPIO_AF7_USART1
#define ZS040_RX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()

#ifndef ZS040_BAUD_RATE
#define ZS040_BAUD_RATE 9600U
#endif

/* HC-06 legacy AT syntax; sent without CR/LF while disconnected. */
#define ZS040_DEVICE_NAME "MoonCar"
#define ZS040_PAIRING_PIN "0000"
#define ZS040_AT_BOOT_MS 1000U
#define ZS040_AT_TIMEOUT_MS 1000U

typedef enum {
    ZS040_CONFIG_WAITING = 0,
    ZS040_CONFIG_OK,
    ZS040_CONFIG_NO_AT,
    ZS040_CONFIG_NAME_FAILED,
    ZS040_CONFIG_PIN_FAILED,
    ZS040_CONFIG_UART_ERROR
} ZS040ConfigStatus;
extern volatile ZS040ConfigStatus zs040_config_status;
/* Call periodically after Init; motors must remain stopped while WAITING.
 * Failed configuration still allows normal control after the startup attempt. */
void ZS040_Update(void);

/* USART1: PA9 TX -> module RX, PA10 RX <- module TX; 8N1 data mode.
 * ZS-040 is a carrier label: verify the fitted module's actual baud rate.
 * Uses CMSIS registers; no extra HAL UART package is required. */
void ZS040_Init(void);
void ZS040_RxIRQ(void);
uint8_t ZS040_Read(uint8_t *byte, uint32_t *received_ms);
/* Copies one reply for nonblocking transmission. Returns 0 if busy. */
uint8_t ZS040_Write(const char *bytes, uint8_t length);
/* Consumes transport fault and discards queued commands. */
uint8_t ZS040_TakeFault(void);
#endif
