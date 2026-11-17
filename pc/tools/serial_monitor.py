#!/usr/bin/env python3
"""串口帧监视器：实时解码 STM32 发来的 / 发往 STM32 的帧。"""
from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

import serial

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from protocol import FrameParser, decode_detection, frame_to_hex, TYPE_DET, TYPE_PING, TYPE_PONG  # noqa: E402


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0.05)
    parser = FrameParser()
    print(f"[monitor] {args.port} @ {args.baud}, Ctrl+C 退出")
    try:
        while True:
            data = ser.read(256)
            if not data:
                continue
            for msg_type, seq, payload in parser.feed(data):
                ts = time.strftime("%H:%M:%S")
                print(f"{ts} type=0x{msg_type:02X} seq={seq} len={len(payload)} "
                      f"raw=[{frame_to_hex(payload)}]")
                if msg_type == TYPE_DET:
                    try:
                        info = decode_detection(payload)
                        print(f"          DET frame_id={info['frame_id']} n={info['n']} "
                              f"targets={info['targets']}")
                    except ValueError as exc:
                        print(f"          DET decode error: {exc}")
                elif msg_type == TYPE_PING:
                    print("          PING")
                elif msg_type == TYPE_PONG:
                    print("          PONG")
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
