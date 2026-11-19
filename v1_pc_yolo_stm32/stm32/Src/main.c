/*
 * Vision-Link STM32 主程序（PlatformIO / STM32Cube HAL）
 *
 * 目标：STM32F103C8T6 @72MHz，USART1 115200，DMA 循环接收 + 空闲中断
 * 编译：pio run
 * 下载：BOOT0=1 复位，pio run -t upload --upload-port COM14，完成后 BOOT0=0 复位
 */
#include "stm32f1xx_hal.h"
#include "app_vision_link.h"
#include "servo_pwm.h"

UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_rx;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();

    VisionLink_Init(&huart1);
    Servo_Init();

    {
        static const char boot_msg[] = "Vision-Link ready\r\n";
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)boot_msg,
                                (uint16_t)(sizeof(boot_msg) - 1u), 100u);
    }

    while (1) {
        VisionLink_Poll();
    }
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        while (1) { }
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) {
        while (1) { }
    }
}

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
}

static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();
    HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
}

static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        while (1) { }
    }
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef gpio = {0};

    if (huart->Instance != USART1) {
        return;
    }

    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_9;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = GPIO_PIN_10;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &gpio);

    hdma_usart1_rx.Instance = DMA1_Channel5;
    hdma_usart1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_usart1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart1_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_rx.Init.Mode = DMA_CIRCULAR;
    hdma_usart1_rx.Init.Priority = DMA_PRIORITY_HIGH;
    if (HAL_DMA_Init(&hdma_usart1_rx) != HAL_OK) {
        while (1) { }
    }
    __HAL_LINKDMA(huart, hdmarx, hdma_usart1_rx);

    HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

void USART1_IRQHandler(void)
{
    VisionLink_OnUartIdle(&huart1);
    HAL_UART_IRQHandler(&huart1);
}

void DMA1_Channel5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_rx);
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void VisionLink_OnDetection(const proto_det_t *det)
{
    static float current_angle = 90.0f;   /* 云台当前角度，90 度朝正前方 */

    if (det->n == 0u) {
        return;                           /* 没有目标时保持当前角度，不抖 */
    }

    uint16_t cx = (uint16_t)(det->targets[0].x + det->targets[0].w / 2u);
    float error = (float)cx - 640.0f;     /* 相对画面中心的像素偏差 */

    /* 死区：目标在中心附近时不动作，避免云台来回抖 */
    if ((error > -20.0f) && (error < 20.0f)) {
        return;
    }

    /* 比例映射：最大 ±30 度（60~120 度），再低通滤波让转动平滑 */
    float target_angle = 90.0f + error * (30.0f / 640.0f);
    current_angle += (target_angle - current_angle) * 0.25f;
    Servo_SetAngle(current_angle);
}
