"""检测后端：YOLOv8 / Haar 人脸 / 运动检测

统一输出 Detection（像素坐标），由 main.py 转成协议 Target。
"""
from __future__ import annotations

from dataclasses import dataclass

import cv2
import numpy as np


@dataclass
class Detection:
    cls_id: int
    label: str
    conf: float
    x: int
    y: int
    w: int
    h: int


class BaseDetector:
    def detect(self, frame: np.ndarray) -> list[Detection]:
        raise NotImplementedError


class YoloDetector(BaseDetector):
    """ultralytics YOLOv8，COCO 80 类开箱即用，无需自己训练。"""

    def __init__(self, model: str = "yolov8n.pt", conf: float = 0.35,
                 imgsz: int = 640, device: str = "cpu") -> None:
        from ultralytics import YOLO

        self.model = YOLO(model)
        self.conf = conf
        self.imgsz = imgsz
        self.device = device

    def detect(self, frame: np.ndarray) -> list[Detection]:
        result = self.model.predict(
            frame, conf=self.conf, imgsz=self.imgsz, device=self.device, verbose=False
        )[0]
        names = result.names
        detections: list[Detection] = []
        for box in result.boxes:
            x1, y1, x2, y2 = (float(v) for v in box.xyxy[0].tolist())
            cls_id = int(box.cls[0])
            conf = float(box.conf[0])
            detections.append(
                Detection(
                    cls_id=cls_id,
                    label=str(names.get(cls_id, cls_id)),
                    conf=conf,
                    x=max(0, int(x1)),
                    y=max(0, int(y1)),
                    w=max(1, int(x2 - x1)),
                    h=max(1, int(y2 - y1)),
                )
            )
        return detections


class HaarFaceDetector(BaseDetector):
    """OpenCV 自带 Haar 人脸检测，零模型下载，适合先跑通链路。"""

    def __init__(self) -> None:
        path = cv2.data.haarcascades + "haarcascade_frontalface_default.xml"
        self.cascade = cv2.CascadeClassifier(path)
        if self.cascade.empty():
            raise RuntimeError("failed to load Haar cascade")

    def detect(self, frame: np.ndarray) -> list[Detection]:
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        faces = self.cascade.detectMultiScale(gray, scaleFactor=1.1, minNeighbors=5,
                                              minSize=(60, 60))
        return [
            Detection(cls_id=0, label="face", conf=0.9, x=int(x), y=int(y),
                      w=int(w), h=int(h))
            for (x, y, w, h) in faces
        ]


class MotionDetector(BaseDetector):
    """背景差分运动检测，静态机位下最省事，完全不需要模型。"""

    def __init__(self, min_area: int = 1200) -> None:
        self.bg = cv2.createBackgroundSubtractorMOG2(history=300, varThreshold=32,
                                                     detectShadows=False)
        self.min_area = min_area

    def detect(self, frame: np.ndarray) -> list[Detection]:
        mask = self.bg.apply(frame)
        mask = cv2.erode(mask, None, iterations=1)
        mask = cv2.dilate(mask, None, iterations=2)
        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        detections: list[Detection] = []
        for contour in contours:
            x, y, w, h = cv2.boundingRect(contour)
            if w * h < self.min_area:
                continue
            detections.append(Detection(cls_id=0, label="motion", conf=0.5,
                                        x=x, y=y, w=w, h=h))
        return detections


def build_detector(cfg: dict) -> BaseDetector:
    backend = str(cfg.get("backend", "yolo")).lower()
    if backend == "yolo":
        try:
            return YoloDetector(
                model=str(cfg.get("model", "yolov8n.pt")),
                conf=float(cfg.get("conf", 0.35)),
                imgsz=int(cfg.get("imgsz", 640)),
                device=str(cfg.get("device", "cpu")),
            )
        except ImportError:
            print("[warn] 未安装 ultralytics，自动改用 motion 检测；"
                  "pip install ultralytics 后可用 YOLO")
            return MotionDetector(min_area=int(cfg.get("min_area", 1200)))
    if backend == "face":
        return HaarFaceDetector()
    if backend == "motion":
        return MotionDetector(min_area=int(cfg.get("min_area", 1200)))
    raise ValueError(f"unknown detector backend: {backend}")
