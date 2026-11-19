#include "app_vision_link.h"
#include "protocol.h"
#include "ringbuf.h"

#include <string.h>

#define RX_DMA_BUF_SIZE 256u

static UART_HandleTypeDef *s_huart;
static uint8_t  s_dma_buf[RX_DMA_BUF_SIZE];
static uint16_t s_old_pos;
static ringbuf_t s_rb;
static proto_parser_t s_parser;
static proto_frame_t  s_frame;
static uint8_t  s_tx_buf[PROTO_MAX_PAYLOAD + 6u];
static uint32_t s_drop_count;
static uint32_t s_frame_count;

void VisionLink_Init(UART_HandleTypeDef *huart)
{
    s_huart = huart;
    s_old_pos = 0u;
    s_drop_count = 0u;
    s_frame_count = 0u;
    ringbuf_init(&s_rb);
    proto_parser_init(&s_parser);
    (void)HAL_UART_Receive_DMA(huart, s_dma_buf, RX_DMA_BUF_SIZE);
    __HAL_UART_ENABLE_IT(huart, UART_IT_IDLE);
}

void VisionLink_OnUartIdle(UART_HandleTypeDef *huart)
{
    if ((huart == NULL) || (huart != s_huart) || (huart->hdmarx == NULL)) {
        return;
    }
    if (!__HAL_UART_GET_FLAG(huart, UART_FLAG_IDLE)) {
        return;
    }
    __HAL_UART_CLEAR_IDLEFLAG(huart);

    uint16_t pos = (uint16_t)(RX_DMA_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart->hdmarx));
    while (s_old_pos != pos) {
        if (!ringbuf_put(&s_rb, s_dma_buf[s_old_pos])) {
            s_drop_count++;
        }
        s_old_pos = (uint16_t)((s_old_pos + 1u) % RX_DMA_BUF_SIZE);
    }
}

void VisionLink_Poll(void)
{
    uint8_t byte;
    while (ringbuf_get(&s_rb, &byte)) {
        if (!proto_parser_feed(&s_parser, byte, &s_frame)) {
            continue;
        }
        s_frame_count++;
        if (s_frame.type == PROTO_TYPE_DET) {
            proto_det_t det;
            if (proto_decode_det(&s_frame, &det) == 0) {
                VisionLink_OnDetection(&det);
            }
        } else if (s_frame.type == PROTO_TYPE_PING) {
            (void)VisionLink_Send(PROTO_TYPE_PONG, s_frame.seq,
                                  s_frame.payload, s_frame.len);
        }
    }
}

HAL_StatusTypeDef VisionLink_Send(uint8_t type, uint8_t seq,
                                  const uint8_t *payload, uint8_t len)
{
    uint16_t n = proto_build_frame(s_tx_buf, sizeof(s_tx_buf), type, seq, payload, len);
    if (n == 0u) {
        return HAL_ERROR;
    }
    return HAL_UART_Transmit(s_huart, s_tx_buf, n, 10u);
}

uint32_t VisionLink_GetDropCount(void)
{
    return s_drop_count;
}

uint32_t VisionLink_GetFrameCount(void)
{
    return s_frame_count;
}

__weak void VisionLink_OnDetection(const proto_det_t *det)
{
    (void)det;
}
