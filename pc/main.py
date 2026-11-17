#!/usr/bin/env python3
"""Vision-Link PC 端主程序：取帧 → 检测 → 打包 → 串口发送。

用法：
    python main.py                 # 按 config.yaml 跑
    python main.py --no-serial     # 只预览，不发送
    python main.py --record demo.mp4
"""
from __future__ import annotations

import argparse
import sys
import time
from collections import deque
from pathlib import Path

import cv2
import yaml

from detector import build_detector
from protocol import Target
from serial_link import SerialLink

# 协议统一使用 1280x720 逻辑坐标，STM32 端按这个分辨率映射舵机
LOGICAL_WIDTH = 1280
LOGICAL_HEIGHT = 720


def load_config(path: str) -> dict:
    with open(path, "r", encoding="utf-8") as f:
        return yaml.safe_load(f)


def open_capture(cfg: dict) -> cv2.VideoCapture:
    src = cfg.get("source", {})
    if src.get("type", "camera") == "video":
        path = src.get("path", "")
        if not path:
            raise ValueError("source.type=video 但 source.path 为空")
        cap = cv2.VideoCapture(path)
    else:
        cap = cv2.VideoCapture(int(src.get("index", 0)), cv2.CAP_DSHOW)
        width = int(src.get("width", 1280))
        height = int(src.get("height", 720))
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
        fourcc = str(src.get("fourcc", "MJPG"))
        if fourcc:
            cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*fourcc))
        cap.set(cv2.CAP_PROP_FPS, int(src.get("fps", 30)))
    if not cap.isOpened():
        raise RuntimeError("摄像头/视频打开失败，检查 config.yaml 或先跑 tools/list_cameras.py")
    return cap


def draw(frame, detections) -> None:
    for det in detections:
        color = (0, 220, 0)
        cv2.rectangle(frame, (det.x, det.y), (det.x + det.w, det.y + det.h), color, 2)
        text = f"{det.label} {det.conf:.2f}"
        cv2.putText(frame, text, (det.x, max(18, det.y - 6)),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.55, color, 1, cv2.LINE_AA)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", default=str(Path(__file__).with_name("config.yaml")))
    parser.add_argument("--no-serial", action="store_true")
    parser.add_argument("--no-show", action="store_true")
    parser.add_argument("--record", default="", help="录制预览视频到文件")
    parser.add_argument("--max-frames", type=int, default=0)
    args = parser.parse_args()

    cfg = load_config(args.config)
    detector = build_detector(cfg.get("detector", {}))
    cap = open_capture(cfg)

    serial_cfg = cfg.get("serial", {})
    link = None
    if serial_cfg.get("enabled", True) and not args.no_serial:
        link = SerialLink(port=str(serial_cfg["port"]), baud=int(serial_cfg.get("baud", 115200)))
        print(f"[serial] opened {serial_cfg['port']} @ {serial_cfg.get('baud', 115200)}")

    writer = None
    if args.record:
        fps = float(cap.get(cv2.CAP_PROP_FPS) or 25)
        w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        writer = cv2.VideoWriter(args.record, cv2.VideoWriter_fourcc(*"mp4v"), fps, (w, h))

    proc_ms: deque[float] = deque(maxlen=300)
    frame_id = 0
    frame_count = 0
    sent = 0
    t_start = time.perf_counter()
    fps = 0.0
    last_t = t_start

    print("[run] q 退出")
    try:
        while True:
            ok, frame = cap.read()
            if not ok:
                print("[warn] 读帧失败/视频结束")
                break

            t0 = time.perf_counter()
            detections = detector.detect(frame)
            frame_h, frame_w = frame.shape[:2]
            scale_x = LOGICAL_WIDTH / frame_w
            scale_y = LOGICAL_HEIGHT / frame_h
            targets = [
                Target(cls_id=d.cls_id, conf=int(round(d.conf * 100)),
                       x=int(d.x * scale_x), y=int(d.y * scale_y),
                       w=int(d.w * scale_x), h=int(d.h * scale_y))
                for d in detections[:8]
            ]

            send_ms = 0.0
            if link is not None:
                send_ms = link.send_detection(frame_id, targets)
                sent += 1

            proc_ms.append((time.perf_counter() - t0) * 1000.0)
            draw(frame, detections)

            now = time.perf_counter()
            fps = 0.9 * fps + 0.1 * (1.0 / max(1e-6, now - last_t)) if fps else 1.0 / max(1e-6, now - last_t)
            last_t = now

            if not args.no_show:
                info = f"FPS {fps:5.1f} | det {len(detections)} | {proc_ms[-1]:5.1f} ms"
                cv2.putText(frame, info, (12, 28), cv2.FONT_HERSHEY_SIMPLEX, 0.7,
                            (0, 255, 255), 2, cv2.LINE_AA)
                cv2.imshow("Vision-Link", frame)
                if cv2.waitKey(1) & 0xFF == ord("q"):
                    break

            if writer is not None:
                writer.write(frame)

            frame_id = (frame_id + 1) & 0xFF
            frame_count += 1
            if args.max_frames and frame_count >= args.max_frames:
                break
    finally:
        cap.release()
        if writer is not None:
            writer.release()
        if link is not None:
            link.close()
        cv2.destroyAllWindows()

    elapsed = time.perf_counter() - t_start
    if proc_ms:
        avg = sum(proc_ms) / len(proc_ms)
        print(f"[done] {elapsed:.1f}s, frames={frame_count}, sent={sent}, "
              f"fps={frame_count / elapsed:.1f}, avg proc={avg:.1f} ms, "
              f"p95={sorted(proc_ms)[int(len(proc_ms) * 0.95) - 1]:.1f} ms")
    return 0


if __name__ == "__main__":
    sys.exit(main())
