#include "protocol.h"
#include "crc8.h"

#include <string.h>

static uint32_t s_crc_errors = 0u;

void proto_parser_init(proto_parser_t *p)
{
    memset(p, 0, sizeof(*p));
    p->state = PROTO_ST_H1;
}

int proto_parser_feed(proto_parser_t *p, uint8_t byte, proto_frame_t *out)
{
    switch (p->state) {
    case PROTO_ST_H1:
        if (byte == PROTO_HEAD1) {
            p->state = PROTO_ST_H2;
        }
        break;

    case PROTO_ST_H2:
        if (byte == PROTO_HEAD2) {
            p->state = PROTO_ST_LEN;
        } else if (byte != PROTO_HEAD1) {
            p->state = PROTO_ST_H1;
        }
        break;

    case PROTO_ST_LEN:
        if (byte > PROTO_MAX_PAYLOAD) {
            p->state = PROTO_ST_H1;
        } else {
            p->len = byte;
            p->state = PROTO_ST_TYPE;
        }
        break;

    case PROTO_ST_TYPE:
        p->type = byte;
        p->state = PROTO_ST_SEQ;
        break;

    case PROTO_ST_SEQ:
        p->seq = byte;
        p->idx = 0u;
        p->state = (p->len > 0u) ? PROTO_ST_PAYLOAD : PROTO_ST_CRC;
        break;

    case PROTO_ST_PAYLOAD:
        p->payload[p->idx++] = byte;
        if (p->idx >= p->len) {
            p->state = PROTO_ST_CRC;
        }
        break;

    case PROTO_ST_CRC: {
        uint8_t buf[3u + PROTO_MAX_PAYLOAD];
        buf[0] = p->len;
        buf[1] = p->type;
        buf[2] = p->seq;
        memcpy(&buf[3], p->payload, p->len);
        uint8_t crc = crc8_calc(buf, (uint16_t)(3u + p->len));
        p->state = PROTO_ST_H1;
        if (crc == byte) {
            out->type = p->type;
            out->seq = p->seq;
            out->len = p->len;
            memcpy(out->payload, p->payload, p->len);
            return 1;
        }
        s_crc_errors++;
        break;
    }

    default:
        p->state = PROTO_ST_H1;
        break;
    }
    return 0;
}

int proto_decode_det(const proto_frame_t *f, proto_det_t *out)
{
    if ((f->type != PROTO_TYPE_DET) || (f->len < 2u)) {
        return -1;
    }
    uint8_t n = f->payload[1];
    if (n > PROTO_MAX_TARGETS) {
        return -2;
    }
    if (f->len != (uint8_t)(2u + n * PROTO_TARGET_SIZE)) {
        return -3;
    }

    out->frame_id = f->payload[0];
    out->n = n;
    const uint8_t *p = &f->payload[2];
    for (uint8_t i = 0u; i < n; i++) {
        out->targets[i].cls  = p[0];
        out->targets[i].conf = p[1];
        out->targets[i].x = (uint16_t)((uint16_t)p[2] | ((uint16_t)p[3] << 8));
        out->targets[i].y = (uint16_t)((uint16_t)p[4] | ((uint16_t)p[5] << 8));
        out->targets[i].w = (uint16_t)((uint16_t)p[6] | ((uint16_t)p[7] << 8));
        out->targets[i].h = (uint16_t)((uint16_t)p[8] | ((uint16_t)p[9] << 8));
        p += PROTO_TARGET_SIZE;
    }
    return 0;
}

uint16_t proto_build_frame(uint8_t *buf, uint16_t buf_size, uint8_t type,
                           uint8_t seq, const uint8_t *payload, uint8_t len)
{
    if ((uint16_t)(len + 6u) > buf_size) {
        return 0u;
    }
    buf[0] = PROTO_HEAD1;
    buf[1] = PROTO_HEAD2;
    buf[2] = len;
    buf[3] = type;
    buf[4] = seq;
    if (len > 0u) {
        memcpy(&buf[5], payload, len);
    }
    buf[5u + len] = crc8_calc(&buf[2], (uint16_t)(3u + len));
    return (uint16_t)(6u + len);
}

uint32_t proto_crc_error_count(void)
{
    return s_crc_errors;
}
