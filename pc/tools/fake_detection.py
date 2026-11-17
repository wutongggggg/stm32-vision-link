#!/usr/bin/env python3
"""不接摄像头，直接向 STM32 发模拟检测帧，用来单独验证 PC→STM32 执行链路。

用法：
    python tools/fake_detection.py --port COM3 --seconds 10
"""
from __future__ import annotations

import argparse
import math
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from protocol import Target  # noqa: E402
from serial_link import SerialLink  # noqa: E402


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--seconds", type=float, default=10.0)
    ap.add_argument("--fps", type=float, default=20.0)
    args = ap.parse_args()

    link = SerialLink(args.port, args.baud)
    period = 1.0 / args.fps
    t0 = time.perf_counter()
    frame = 0
    try:
        while time.perf_counter() - t0 < args.seconds:
            phase = (time.perf_counter() - t0) * 2.0 * math.pi / 4.0  # 4 秒一个来回
            x = int((math.sin(phase) * 0.5 + 0.5) * (1280 - 100))
            target = Target(cls_id=0, conf=90, x=x, y=240, w=100, h=80)
            link.send_detection(frame & 0xFF, [target])
            frame += 1
            time.sleep(period)
    finally:
        link.close()
    print(f"sent {frame} frames in {time.perf_counter() - t0:.1f}s")
    return 0


if __name__ == "__main__":
    sys.exit(main())
