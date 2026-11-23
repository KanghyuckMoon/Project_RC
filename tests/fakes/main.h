#ifndef TEST_MAIN_H
#define TEST_MAIN_H
/* Host-only peripheral facade. This does not model electrical behavior. */
#include <stdint.h>
#include <assert.h>
typedef struct { uint32_t Pin, Mode, Pull, Speed, Alternate; } GPIO_InitTypeDef;
typedef struct { int unused; } GPIO_TypeDef;
typedef struct {
    void *Instance;
    struct { uint32_t Prescaler, CounterMode, Period, ClockDivision; } Init;
} TIM_HandleTypeDef;
typedef struct { volatile uint32_t SR, DR, BRR, CR1, CR2, CR3; } FakeUart;
typedef struct { uint32_t CFGR; } FakeRcc;
extern FakeUart fake_uart;
extern FakeRcc fake_rcc;
extern uint32_t fake_tick, fake_us, fake_irq, fake_pending, fake_echo, fake_trig, fake_last_trig;
#define USART1 (&fake_uart)
#define RCC (&fake_rcc)
#define GPIOA ((void *)1)
#define GPIOB ((void *)2)
#define GPIOC ((void *)3)
#define TIM5 ((void *)5)
#define GPIO_PIN_0 1U
#define GPIO_PIN_1 2U
#define GPIO_PIN_4 (1U << 4)
#define GPIO_PIN_5 (1U << 5)
#define GPIO_PIN_6 (1U << 6)
#define GPIO_PIN_7 (1U << 7)
#define GPIO_PIN_9 (1U << 9)
#define GPIO_PIN_10 (1U << 10)
#define GPIO_PIN_RESET 0U
#define GPIO_PIN_SET 1U
#define US_FRONT_TRIG_Pin GPIO_PIN_0
#define US_FRONT_TRIG_GPIO_Port GPIOB
#define US_FRONT_ECHO_Pin GPIO_PIN_1
#define US_FRONT_ECHO_GPIO_Port GPIOB
#define US_LEFT_TRIG_Pin GPIO_PIN_4
#define US_LEFT_TRIG_GPIO_Port GPIOC
#define US_LEFT_ECHO_Pin GPIO_PIN_5
#define US_LEFT_ECHO_GPIO_Port GPIOC
#define US_RIGHT_TRIG_Pin GPIO_PIN_6
#define US_RIGHT_TRIG_GPIO_Port GPIOC
#define US_RIGHT_ECHO_Pin GPIO_PIN_7
#define US_RIGHT_ECHO_GPIO_Port GPIOC
#define GPIO_MODE_OUTPUT_PP 1U
#define GPIO_MODE_IT_RISING_FALLING 2U
#define GPIO_MODE_AF_PP 3U
#define GPIO_SPEED_FREQ_LOW 0U
#define GPIO_SPEED_FREQ_VERY_HIGH 3U
#define GPIO_PULLDOWN 2U
#define GPIO_PULLUP 1U
#define GPIO_AF7_USART1 7U
#define TIM_COUNTERMODE_UP 0U
#define TIM_CLOCKDIVISION_DIV1 0U
#define RCC_CFGR_PPRE1 0x1c00U
#define EXTI1_IRQn 7
#define USART1_IRQn 37
#define RESET 0U
#define HAL_OK 0
#define USART_SR_PE (1U << 0)
#define USART_SR_FE (1U << 1)
#define USART_SR_NE (1U << 2)
#define USART_SR_ORE (1U << 3)
#define USART_SR_RXNE (1U << 5)
#define USART_SR_TC (1U << 6)
#define USART_SR_TXE (1U << 7)
#define USART_CR1_RE (1U << 2)
#define USART_CR1_TE (1U << 3)
#define USART_CR1_RXNEIE (1U << 5)
#define USART_CR1_PEIE (1U << 8)
#define USART_CR1_UE (1U << 13)
#define USART_CR3_EIE 1U
#define __HAL_RCC_GPIOA_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM5_CLK_ENABLE() ((void)0)
#define __HAL_RCC_USART1_CLK_ENABLE() ((void)0)
#define __HAL_RCC_USART1_FORCE_RESET() ((void)0)
#define __HAL_RCC_USART1_RELEASE_RESET() ((void)0)
#define __HAL_GPIO_EXTI_CLEAR_IT(pin) (fake_pending &= ~(uint32_t)(pin))
#define __HAL_TIM_GET_COUNTER(timer) ((void)(timer), fake_us++)
static inline uint32_t HAL_GetTick(void) { return fake_tick; }
static inline uint32_t HAL_RCC_GetPCLK1Freq(void) { return 50000000U; }
static inline uint32_t HAL_RCC_GetPCLK2Freq(void) { return 100000000U; }
static inline uint32_t __get_PRIMASK(void) { return fake_irq; }
static inline void __disable_irq(void) { fake_irq = 1; }
static inline void __set_PRIMASK(uint32_t state) { fake_irq = state; }
static inline void __DMB(void) { }
static inline void Error_Handler(void) { assert(0); }
static inline void HAL_GPIO_Init(void *port, GPIO_InitTypeDef *init)
{ (void)port; (void)init; }
static inline void HAL_GPIO_WritePin(void *port, uint32_t pin, uint32_t state)
{
    (void)port;
    if (state == GPIO_PIN_SET)
    {
        assert(!fake_trig);
        fake_trig = fake_last_trig = pin;
    }
    else
        fake_trig &= ~pin;
}
static inline uint32_t HAL_GPIO_ReadPin(void *port, uint32_t pin)
{ (void)port; return (fake_echo & pin) != 0U; }
static inline int HAL_TIM_Base_Start(TIM_HandleTypeDef *timer)
{ (void)timer; return HAL_OK; }
void HAL_GPIO_EXTI_Callback(uint16_t pin);
static inline void HAL_GPIO_EXTI_IRQHandler(uint16_t pin)
{
    if (!(fake_pending & pin)) return;
    __HAL_GPIO_EXTI_CLEAR_IT(pin);
    HAL_GPIO_EXTI_Callback(pin);
}
static inline void HAL_NVIC_SetPriority(int irq, int preempt, int sub)
{ (void)irq; (void)preempt; (void)sub; }
static inline void HAL_NVIC_EnableIRQ(int irq) { (void)irq; }
#endif
