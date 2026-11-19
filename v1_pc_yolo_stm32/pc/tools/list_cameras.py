#!/usr/bin/env python3
"""枚举摄像头/采集卡，显示设备名和序号。

Windows 上采集卡通常叫 "USB Video"，笔记本自带摄像头叫 "Integrated Webcam"。
"""
import cv2

try:
    from pygrabber.dshow_graph import FilterGraph
except ImportError:
    FilterGraph = None


def main() -> None:
    names = []
    if FilterGraph is not None:
        try:
            names = FilterGraph().get_input_devices()
        except Exception as exc:
            print(f"[warn] DirectShow 设备名枚举失败: {exc}")

    print("index | device name | resolution | backend")
    print("-" * 60)
    for index in range(6):
        cap = cv2.VideoCapture(index, cv2.CAP_DSHOW)
        if not cap.isOpened():
            cap.release()
            continue
        cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1920)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 1080)
        ok, frame = cap.read()
        name = names[index] if index < len(names) else "?"
        if ok and frame is not None:
            h, w = frame.shape[:2]
            print(f"{index:5d} | {name} | {w}x{h} | DSHOW")
        else:
            print(f"{index:5d} | {name} | opened but no frame | DSHOW")
        cap.release()


if __name__ == "__main__":
    main()
