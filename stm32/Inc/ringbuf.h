#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdint.h>

#define RINGBUF_SIZE 512u   /* 必须是 2 的幂 */

typedef struct {
    uint8_t           buf[RINGBUF_SIZE];
    volatile uint16_t head;   /* 只由写入方（中断）修改 */
    volatile uint16_t tail;   /* 只由读取方（主循环）修改 */
} ringbuf_t;

void ringbuf_init(ringbuf_t *rb);

/* 中断里调用；满了返回 0 */
int ringbuf_put(ringbuf_t *rb, uint8_t byte);

/* 主循环调用；空返回 0 */
int ringbuf_get(ringbuf_t *rb, uint8_t *byte);

uint16_t ringbuf_count(const ringbuf_t *rb);

#endif /* RINGBUF_H */
