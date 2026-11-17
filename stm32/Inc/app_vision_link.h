#ifndef APP_VISION_LINK_H
#define APP_VISION_LINK_H

#include "stm32f1xx_hal.h"
#include "protocol.h"

/* 初始化：启动 USART DMA 接收 + 打开空闲中断 */
void VisionLink_Init(UART_HandleTypeDef *huart);

/* 在 USARTx_IRQHandler 里调用（必须早于 HAL_UART_IRQHandler） */
void VisionLink_OnUartIdle(UART_HandleTypeDef *huart);

/* 主循环里调用：解析环形缓冲中的字节，分发帧 */
void VisionLink_Poll(void);

/* 发送一帧（阻塞，最长 10ms） */
HAL_StatusTypeDef VisionLink_Send(uint8_t type, uint8_t seq,
                                  const uint8_t *payload, uint8_t len);

/* 接收缓冲溢出计数（用于压测/指标） */
uint32_t VisionLink_GetDropCount(void);
uint32_t VisionLink_GetFrameCount(void);

/* 收到检测结果时的弱回调，用户在应用层重写 */
void VisionLink_OnDetection(const proto_det_t *det);

#endif /* APP_VISION_LINK_H */
