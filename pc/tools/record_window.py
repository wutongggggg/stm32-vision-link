#!/usr/bin/env python3
"""录制 Vision-Link 预览窗口（带检测框/FPS 的完整画面）。

用法：
    python tools/record_window.py --duration 60 --output demo.mp4
运行后 3 秒倒计时，请让 Vision-Link 窗口保持可见，然后开始演示。
"""
from __future__ import annotations

import argparse
import ctypes
import time
from ctypes import wintypes

import cv2
import mss
import numpy as np
import pygetwindow as gw


def client_rect(win):
    """返回窗口客户区在屏幕上的 (left, top, width, height)，去掉标题栏边框。"""
    hwnd = win._hWnd
    rect = wintypes.RECT()
    ctypes.windll.user32.GetClientRect(hwnd, ctypes.byref(rect))
    point = wintypes.POINT(0, 0)
    ctypes.windll.user32.ClientToScreen(hwnd, ctypes.byref(point))
    return point.x, point.y, rect.right, rect.bottom


def find_window(title: str):
    windows = [w for w in gw.getAllWindows() if title.lower() in w.title.lower() and w.visible]
    if not windows:
        raise RuntimeError(f"找不到窗口标题包含 {title!r} 的窗口，请先运行 main.py")
    return windows[0]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--title", default="Vision-Link")
    ap.add_argument("--duration", type=float, default=60.0)
    ap.add_argument("--fps", type=float, default=30.0)
    ap.add_argument("--output", default="demo.mp4")
    ap.add_argument("--delay", type=float, default=3.0)
    args = ap.parse_args()

    win = find_window(args.title)
    try:
        win.activate()
    except Exception:
        pass
    time.sleep(0.5)
    left, top, width, height = client_rect(win)
    print(f"录制窗口: {win.title!r} 区域 {width}x{height} @ ({left},{top})")
    for i in range(int(args.delay), 0, -1):
        print(f"{i}...")
        time.sleep(1)

    writer = cv2.VideoWriter(args.output, cv2.VideoWriter_fourcc(*"mp4v"), args.fps,
                             (width, height))
    if not writer.isOpened():
        raise RuntimeError("无法创建视频文件，检查输出路径/编码器")

    region = {"left": left, "top": top, "width": width, "height": height}
    frame_count = 0
    t0 = time.perf_counter()
    last_report = t0
    print("开始录制，按 Ctrl+C 可提前结束")
    try:
        with mss.MSS() as sct:
            while time.perf_counter() - t0 < args.duration:
                shot = sct.grab(region)
                frame = np.array(shot)[:, :, :3]  # BGRA -> BGR
                writer.write(frame)
                frame_count += 1
                now = time.perf_counter()
                if now - last_report >= 5.0:
                    print(f"  {now - t0:4.1f}s / {args.duration:.0f}s，已录 {frame_count} 帧")
                    last_report = now
    except KeyboardInterrupt:
        print("手动停止")
    finally:
        writer.release()

    real_fps = frame_count / max(1e-6, time.perf_counter() - t0)
    print(f"完成: {args.output}，{frame_count} 帧，平均 {real_fps:.1f} fps")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
