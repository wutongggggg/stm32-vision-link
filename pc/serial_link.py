"""串口发送封装。"""
from __future__ import annotations

import time

import serial

from protocol import Target, build_frame, pack_detection, TYPE_DET


class SerialLink:
    def __init__(self, port: str, baud: int = 115200) -> None:
        self.ser = serial.Serial(port=port, baudrate=baud, timeout=0.01)
        self.seq = 0

    def _send(self, msg_type: int, payload: bytes) -> float:
        """返回从打包到写串口完成的耗时（毫秒）。"""
        t0 = time.perf_counter()
        self.seq = (self.seq + 1) & 0xFF
        self.ser.write(build_frame(msg_type, self.seq, payload))
        self.ser.flush()
        return (time.perf_counter() - t0) * 1000.0

    def send_detection(self, frame_id: int, targets: list[Target]) -> float:
        return self._send(TYPE_DET, pack_detection(frame_id, targets))

    def close(self) -> None:
        try:
            self.ser.close()
        except Exception:
            pass
