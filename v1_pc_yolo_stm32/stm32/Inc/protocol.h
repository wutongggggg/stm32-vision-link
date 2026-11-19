#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define PROTO_HEAD1        0xAAu
#define PROTO_HEAD2        0x55u
#define PROTO_MAX_PAYLOAD  96u
#define PROTO_MAX_TARGETS  8u
#define PROTO_TARGET_SIZE  10u

#define PROTO_TYPE_DET     0x01u
#define PROTO_TYPE_PING    0x02u
#define PROTO_TYPE_PONG    0x03u
#define PROTO_TYPE_ACK     0x04u

typedef struct {
    uint8_t  cls;
    uint8_t  conf;      /* 0-100 */
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
} proto_target_t;

typedef struct {
    uint8_t        frame_id;
    uint8_t        n;
    proto_target_t targets[PROTO_MAX_TARGETS];
} proto_det_t;

typedef struct {
    uint8_t type;
    uint8_t seq;
    uint8_t len;
    uint8_t payload[PROTO_MAX_PAYLOAD];
} proto_frame_t;

typedef enum {
    PROTO_ST_H1 = 0,
    PROTO_ST_H2,
    PROTO_ST_LEN,
    PROTO_ST_TYPE,
    PROTO_ST_SEQ,
    PROTO_ST_PAYLOAD,
    PROTO_ST_CRC
} proto_state_t;

typedef struct {
    proto_state_t state;
    uint8_t len;
    uint8_t type;
    uint8_t seq;
    uint8_t idx;
    uint8_t payload[PROTO_MAX_PAYLOAD];
} proto_parser_t;

void proto_parser_init(proto_parser_t *p);

/* 喂入一个字节；解析出完整且 CRC 正确的帧时返回 1，并把帧写入 *out */
int proto_parser_feed(proto_parser_t *p, uint8_t byte, proto_frame_t *out);

/* 把 DET 负载解码到 proto_det_t；返回 0 成功，<0 失败 */
int proto_decode_det(const proto_frame_t *f, proto_det_t *out);

/* 组帧到 buf，返回帧总长度；buf 不够返回 0 */
uint16_t proto_build_frame(uint8_t *buf, uint16_t buf_size, uint8_t type,
                           uint8_t seq, const uint8_t *payload, uint8_t len);

uint32_t proto_crc_error_count(void);

#endif /* PROTOCOL_H */
