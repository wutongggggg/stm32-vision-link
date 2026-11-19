"""Vision-Link 串口帧协议（PC 端实现）

帧格式：AA 55 | LEN | TYPE | SEQ | PAYLOAD | CRC8
CRC8: poly=0x07, init=0x00，覆盖 LEN/TYPE/SEQ/PAYLOAD
"""
from __future__ import annotations

import struct
from dataclasses import dataclass

HEAD1 = 0xAA
HEAD2 = 0x55

TYPE_DET = 0x01
TYPE_PING = 0x02
TYPE_PONG = 0x03
TYPE_ACK = 0x04

MAX_TARGETS = 8
MAX_PAYLOAD = 96
TARGET_SIZE = 10  # cls(1) conf(1) x(2) y(2) w(2) h(2)


def crc8(data: bytes, poly: int = 0x07, init: int = 0x00) -> int:
    crc = init
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = ((crc << 1) ^ poly) & 0xFF if (crc & 0x80) else ((crc << 1) & 0xFF)
    return crc


@dataclass
class Target:
    cls_id: int
    conf: int          # 0-100
    x: int
    y: int
    w: int
    h: int

    def pack(self) -> bytes:
        return struct.pack(
            "<BBHHHH",
            self.cls_id & 0xFF,
            max(0, min(100, int(self.conf))),
            max(0, min(0xFFFF, int(self.x))),
            max(0, min(0xFFFF, int(self.y))),
            max(0, min(0xFFFF, int(self.w))),
            max(0, min(0xFFFF, int(self.h))),
        )


def pack_detection(frame_id: int, targets: list[Target]) -> bytes:
    """打包 DET 负载：frame_id(1) + n(1) + n * target(10B)"""
    targets = targets[:MAX_TARGETS]
    body = struct.pack("<BB", frame_id & 0xFF, len(targets))
    return body + b"".join(t.pack() for t in targets)


def decode_detection(payload: bytes) -> dict:
    """解析 DET 负载，返回 {frame_id, n, targets: [...]}"""
    if len(payload) < 2:
        raise ValueError("payload too short")
    frame_id, n = payload[0], payload[1]
    if len(payload) != 2 + n * TARGET_SIZE:
        raise ValueError(f"bad DET length: {len(payload)} for n={n}")
    targets = []
    off = 2
    for _ in range(n):
        cls_id, conf, x, y, w, h = struct.unpack_from("<BBHHHH", payload, off)
        targets.append(
            {"cls": cls_id, "conf": conf, "x": x, "y": y, "w": w, "h": h}
        )
        off += TARGET_SIZE
    return {"frame_id": frame_id, "n": n, "targets": targets}


def build_frame(msg_type: int, seq: int, payload: bytes = b"") -> bytes:
    if len(payload) > MAX_PAYLOAD:
        raise ValueError(f"payload too large: {len(payload)} > {MAX_PAYLOAD}")
    head = struct.pack("<BBBBB", HEAD1, HEAD2, len(payload), msg_type & 0xFF, seq & 0xFF)
    crc = crc8(head[2:] + payload)
    return head + payload + bytes([crc])


class FrameParser:
    """增量帧解析器：喂任意字节流，返回 [(type, seq, payload), ...]"""

    def __init__(self) -> None:
        self.reset()

    def reset(self) -> None:
        self.state = 0          # 0=H1 1=H2 2=LEN 3=TYPE 4=SEQ 5=PAYLOAD 6=CRC
        self.length = 0
        self.type = 0
        self.seq = 0
        self.payload = bytearray()

    def feed(self, data: bytes) -> list[tuple[int, int, bytes]]:
        frames: list[tuple[int, int, bytes]] = []
        for byte in data:
            if self.state == 0:
                if byte == HEAD1:
                    self.state = 1
            elif self.state == 1:
                if byte == HEAD2:
                    self.state = 2
                elif byte != HEAD1:
                    self.state = 0
            elif self.state == 2:
                if byte > MAX_PAYLOAD:
                    self.state = 0
                else:
                    self.length = byte
                    self.state = 3
            elif self.state == 3:
                self.type = byte
                self.state = 4
            elif self.state == 4:
                self.seq = byte
                self.payload = bytearray()
                self.state = 5 if self.length else 6
            elif self.state == 5:
                self.payload.append(byte)
                if len(self.payload) >= self.length:
                    self.state = 6
            elif self.state == 6:
                expected = crc8(bytes([self.length, self.type, self.seq]) + bytes(self.payload))
                if expected == byte:
                    frames.append((self.type, self.seq, bytes(self.payload)))
                self.state = 0
        return frames


def frame_to_hex(frame: bytes) -> str:
    return " ".join(f"{b:02X}" for b in frame)
