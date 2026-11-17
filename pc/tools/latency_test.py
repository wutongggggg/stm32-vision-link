#!/usr/bin/env python3
"""PING/PONG 串口链路延迟测试。

STM32 端收到 PING 后原样回 PONG（app_vision_link.c 已实现）。
"""
from __future__ import annotations

import argparse
import statistics
import struct
import sys
import time
from pathlib import Path

import serial

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from protocol import HEAD1, HEAD2, TYPE_PING, TYPE_PONG, build_frame, crc8  # noqa: E402


def read_frame(ser: serial.Serial, timeout: float):
    deadline = time.perf_counter() + timeout
    state = 0
    length = msg_type = seq = 0
    payload = bytearray()
    while time.perf_counter() < deadline:
        chunk = ser.read(1)
        if not chunk:
            continue
        byte = chunk[0]
        if state == 0:
            state = 1 if byte == HEAD1 else 0
        elif state == 1:
            state = 2 if byte == HEAD2 else (1 if byte == HEAD1 else 0)
        elif state == 2:
            length, state = (byte, 3) if byte <= 96 else (0, 0)
        elif state == 3:
            msg_type, state = byte, 4
        elif state == 4:
            seq, payload, state = byte, bytearray(), (5 if length else 6)
        elif state == 5:
            payload.append(byte)
            if len(payload) >= length:
                state = 6
        elif state == 6:
            want = crc8(bytes([length, msg_type, seq]) + bytes(payload))
            if want == byte:
                return msg_type, seq, bytes(payload)
            state = 0
    return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--count", type=int, default=100)
    args = ap.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0.001)
    ser.reset_input_buffer()
    rtts = []
    lost = 0
    try:
        for i in range(args.count):
            payload = struct.pack("<I", i)
            ser.write(build_frame(TYPE_PING, i & 0xFF, payload))
            ser.flush()
            t0 = time.perf_counter()
            reply = read_frame(ser, timeout=0.5)
            if reply is None or reply[0] != TYPE_PONG:
                lost += 1
                continue
            rtts.append((time.perf_counter() - t0) * 1000.0)
            time.sleep(0.01)
    finally:
        ser.close()

    if not rtts:
        print("没有收到 PONG，检查接线/波特率/STM32 程序")
        return 1
    rtts.sort()
    print(f"count={len(rtts)} lost={lost}")
    print(f"RTT min={rtts[0]:.2f} ms  avg={statistics.mean(rtts):.2f} ms  "
          f"p50={rtts[len(rtts)//2]:.2f} ms  p95={rtts[int(len(rtts)*0.95)-1]:.2f} ms  "
          f"max={rtts[-1]:.2f} ms")
    return 0


if __name__ == "__main__":
    sys.exit(main())
