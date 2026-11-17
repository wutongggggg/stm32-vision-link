/*
 * ============================================================================
 *  集成示例（仅作参考，整段是注释，不要加入 Keil 工程）
 * ============================================================================
 *
 * 一、main.c 里加三处
 * ----------------------------------------------------------------------------
 *   #include "app_vision_link.h"
 *   #include "servo_pwm.h"
 *
 *   int main(void)
 *   {
 *       HAL_Init();
 *       SystemClock_Config();
 *       MX_GPIO_Init();
 *       MX_DMA_Init();          // 注意：DMA 初始化必须在 USART 之前
 *       MX_USART1_UART_Init();
 *
 *       VisionLink_Init(&huart1);
 *       Servo_Init();
 *
 *       while (1)
 *       {
 *           VisionLink_Poll();
 *       }
 *   }
 *
 * 二、stm32f1xx_it.c 里的 USART1_IRQHandler
 * ----------------------------------------------------------------------------
 *   void USART1_IRQHandler(void)
 *   {
 *       VisionLink_OnUartIdle(&huart1);   // 必须放在 HAL_UART_IRQHandler 之前
 *       HAL_UART_IRQHandler(&huart1);
 *   }
 *
 * 三、应用层回调（写在 main.c 或单独 app.c，不要加 __weak）
 * ----------------------------------------------------------------------------
 *   void VisionLink_OnDetection(const proto_det_t *det)
 *   {
 *       // 示例：用第一个目标的中心 x 控制舵机（假设画面宽 1280）
 *       if (det->n == 0u) {
 *           Servo_SetAngle(90.0f);
 *           return;
 *       }
 *       uint16_t cx = (uint16_t)(det->targets[0].x + det->targets[0].w / 2u);
 *       float angle = 60.0f + (float)cx * 60.0f / 1280.0f;   // 60~120 度
 *       Servo_SetAngle(angle);
 *   }
 *
 * 四、OLED 显示（可选）
 * ----------------------------------------------------------------------------
 *   在 VisionLink_OnDetection 里把 det->n / det->frame_id / 第一个目标坐标
 *   用 sprintf 成字符串，调用洋桃资料包里 OLED_ShowString() 即可。
 */
