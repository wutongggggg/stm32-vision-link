#include "servo_pwm.h"
#include "stm32f1xx_hal.h"

/* 兼容不同版本的 HAL：宏不存在时直接用寄存器实现 */
#ifndef __HAL_AFIO_REMAP_SWJ_NOJTAG
#define __HAL_AFIO_REMAP_SWJ_NOJTAG() \
    do { AFIO->MAPR = (AFIO->MAPR & ~AFIO_MAPR_SWJ_CFG) | AFIO_MAPR_SWJ_CFG_JTAGDISABLE; } while (0)
#endif

#ifndef __HAL_AFIO_REMAP_TIM2_ENABLE
#define __HAL_AFIO_REMAP_TIM2_ENABLE() \
    do { AFIO->MAPR |= AFIO_MAPR_TIM2_REMAP; } while (0)
#endif

static TIM_HandleTypeDef s_htim2;

void Servo_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    TIM_OC_InitTypeDef oc = {0};

    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();

    /* PA15 = JTDI，做 TIM2_CH1 需要关 JTAG（保留 SWD 调试）并完全重映射 TIM2 */
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
    __HAL_AFIO_REMAP_TIM2_ENABLE();

    gpio.Pin = GPIO_PIN_15;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    s_htim2.Instance = TIM2;
    s_htim2.Init.Prescaler = 71u;          /* 72MHz / 72 = 1MHz */
    s_htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    s_htim2.Init.Period = 19999u;          /* 1MHz / 20000 = 50Hz */
    s_htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    s_htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(&s_htim2) != HAL_OK) {
        while (1) { }
    }

    oc.OCMode = TIM_OCMODE_PWM1;
    oc.Pulse = 1500u;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&s_htim2, &oc, TIM_CHANNEL_1) != HAL_OK) {
        while (1) { }
    }

    HAL_TIM_PWM_Start(&s_htim2, TIM_CHANNEL_1);
    Servo_SetPulse(1500u);
}

void Servo_SetPulse(uint16_t us)
{
    if (us < 500u) {
        us = 500u;
    }
    if (us > 2500u) {
        us = 2500u;
    }
    __HAL_TIM_SET_COMPARE(&s_htim2, TIM_CHANNEL_1, us);
}

void Servo_SetAngle(float angle_deg)
{
    if (angle_deg < 0.0f) {
        angle_deg = 0.0f;
    }
    if (angle_deg > 180.0f) {
        angle_deg = 180.0f;
    }
    Servo_SetPulse((uint16_t)(500.0f + angle_deg * 2000.0f / 180.0f));
}
