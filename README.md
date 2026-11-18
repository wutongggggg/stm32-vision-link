# Vision-Link：视觉自动构图云台

一个跑在 **洋桃1号（STM32F103C8T6，64KB Flash / 20KB RAM）** 上的端到端闭环项目：

> 相机/视频 → PC 端 OpenCV + YOLOv8n 检测主体 → 自定义二进制协议（USB 串口）→ STM32 解析 → 舵机云台自动把主体保持在画面中心

这不是"让舵机随便转"的玩具：它解决的是**视频会议、直播跟拍、安防云台**里的自动构图问题——主讲人走动时，云台自动转向，让人始终在画面中央。项目完整走通了真实岗位的分层架构：**感知层（PC）— 通信层（串口协议）— 执行层（MCU 实时控制）**，面试时可以同时讲嵌入式、视觉和系统设计。

## 演示视频

[![Vision-Link 演示](docs/demo_frame.png)](docs/demo.mp4)

[▶ 观看 30 秒演示视频（1280×720 / H.264，无片头片尾）](docs/demo.mp4)

---

## 1. 系统架构

```
┌─────────────────────── PC ───────────────────────┐        ┌──────────── STM32F103C8T6 ────────────┐
│                                                   │        │                                        │
│  相机 ──HDMI──> 绿联采集卡 ──USB(UVC)──┐          │        │                                        │
│                                        ▼          │        │                                        │
│  或视频文件 ────────────────> OpenCV 取帧         │        │                                        │
│                                        │          │        │                                        │
│                              YOLOv8n / Haar 检测  │        │                                        │
│                                        │          │        │                                        │
│                        打包：AA55 | LEN | TYPE |  │ USB    │  USART1 DMA + IDLE 中断                │
│                              SEQ | PAYLOAD | CRC8 │──TTL──>│        ↓                               │
│                                        │          │ 115200 │  环形缓冲 → 状态机解析 → CRC8 校验      │
│                              pyserial 发送        │        │        ↓                               │
│                                                   │        │  舵机 PWM / OLED / LED / 蜂鸣器         │
│                              PING/PONG 回环测延迟  │<───────│  收到 PING 原样回 PONG                 │
└───────────────────────────────────────────────────┘        └────────────────────────────────────────┘
```

**为什么不让 STM32 直接接摄像头？**
F103 没有 DCMI 接口、没有硬件 JPEG、只有 20KB RAM，接 OV7670 会非常痛苦，而且跑不动任何有意义的视觉算法。把图像处理放在 PC、MCU 只做实时控制，是自动驾驶/机器人领域真实存在的分工方式——面试时这就是加分项，不是减分项。

---

## 2. 硬件清单

| 硬件 | 用途 | 备注 |
|---|---|---|
| 洋桃1号开发板（STM32F103C8T6） | 主控 + 执行 | 64KB Flash / 20KB RAM（FlyMcu 实测） |
| 相机 + 绿联 HDMI 采集卡 | 图像源 | UVC 免驱，PC 识别为摄像头 |
| USB 数据线 | 供电 + 串口 | 插 **核心板**上的 MicroUSB（丝印 MicroUSB，支持下载和 USART1）；板上 **USB1 只能供电，不能下载** |
| 舵机 + 云台（5V） | 自动构图执行器 | 接 P7 舵机接口：GND / 5V / PA15 |
| 杜邦线 | 接线 | 舵机建议独立 5V 供电、共地 |

> 没有装舵机也可以先用 OLED/LED 观察目标框；最终形态是"检测主体 → 云台自动转向"的自动构图系统。

---

## 3. 目录结构

```
stm32-vision-link/
├─ pc/                      # PC 端（Python）
│  ├─ main.py               # 主程序：取帧 → 检测 → 发送
│  ├─ detector.py           # 检测后端：YOLO / Haar 人脸 / 运动检测
│  ├─ protocol.py           # 协议打包 + CRC8
│  ├─ serial_link.py        # 串口发送封装
│  ├─ config.yaml           # 配置文件
│  ├─ requirements.txt
│  └─ tools/
│     ├─ list_cameras.py    # 枚举摄像头（找采集卡序号）
│     ├─ latency_test.py    # PING/PONG 测串口 RTT
│     └─ serial_monitor.py  # 串口帧监视/解码
├─ stm32/                   # STM32 端（C，HAL 库）
│  ├─ platformio.ini        # PlatformIO 工程（推荐，无需 Keil）
│  ├─ Inc/
│  │  ├─ protocol.h
│  │  ├─ crc8.h
│  │  ├─ ringbuf.h
│  │  ├─ app_vision_link.h
│  │  └─ servo_pwm.h
│  ├─ Src/
│  │  ├─ main.c              # HAL 初始化 + 中断
│  │  ├─ protocol.c
│  │  ├─ crc8.c
│  │  ├─ ringbuf.c
│  │  ├─ app_vision_link.c
│  │  ├─ servo_pwm.c
│  │  └─ integration_example.c
│  └─ README.md             # CubeMX 配置 + Keil 集成步骤
└─ docs/
   ├─ protocol.md           # 协议字节级说明
   └─ wiring.md             # 接线与引脚
```

---

## 4. 快速开始

### 4.1 PC 端（不接板子也能先跑通）

本机已经建好 `pc\.venv` 并装好全部依赖，可直接：

```bash
cd pc
.venv\Scripts\activate
python main.py --no-serial     # 只预览
```

如果要换一台机器从零装：

```bash
cd pc
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt -i https://pypi.tuna.tsinghua.edu.cn/simple
```

> 第一次只想跑通链路，不必装 YOLO：`config.yaml` 里把 `detector.backend` 改成 `motion` 或 `face` 即可（零模型下载）。想用 YOLO 时再 `pip install ultralytics`（会附带 PyTorch，几百 MB）。

先找到采集卡序号：

```bash
python tools/list_cameras.py
```

改 `config.yaml`（`source.index` 填采集卡序号：**0 = 笔记本自带摄像头，1 = USB Video 采集卡**；`serial.port` 填 COM 口），然后：

```bash
python main.py --no-serial     # 只预览，确认画面和框正常
python main.py                 # 带串口，向 STM32 发送
```

### 4.2 STM32 端

**推荐：PlatformIO（已装好，无需 Keil / CubeMX / ST-Link）**

用 VSCode 打开 `stm32` 文件夹，左下角 PlatformIO → Build 即可；
或者命令行：

```bash
cd stm32
pio run                      # 编译
pio run -t upload --upload-port COM3   # 核心板 MicroUSB 接电脑，ASP 自动下载
```

详细步骤（含核心板 USB 口、ASP 自动下载、FlyMcu 备用）见 [stm32/README.md](stm32/README.md)。

**备选：Keil MDK + CubeMX**，同样见 [stm32/README.md](stm32/README.md)。核心步骤：
1. STM32CubeMX 新建 F103C8Tx 工程，按文档配置时钟/USART1+DMA/TIM2 PWM；
2. 生成 Keil MDK 工程；
3. 把 `stm32/Inc`、`stm32/Src` 下的文件加入工程；
4. 在 `main.c` 和 `stm32f1xx_it.c` 里加三处代码（文档里有现成片段）；
5. Keil 编译下载。

### 4.3 联调

```bash
python tools/serial_monitor.py --port COM3   # 看 STM32 有没有正确解析
python tools/latency_test.py --port COM3     # 测串口链路 RTT
python tools/fake_detection.py --port COM3 --seconds 10   # 不接摄像头，发模拟目标让舵机左右扫
```

---

## 5. 协议摘要

完整说明见 [docs/protocol.md](docs/protocol.md)。

```
AA 55 | LEN | TYPE | SEQ | PAYLOAD | CRC8
```

| 字段 | 长度 | 说明 |
|---|---|---|
| 帧头 | 2 | 固定 `0xAA 0x55` |
| LEN | 1 | PAYLOAD 长度（0–96） |
| TYPE | 1 | 0x01 检测结果 / 0x02 PING / 0x03 PONG / 0x04 ACK |
| SEQ | 1 | 帧序号，0–255 循环 |
| PAYLOAD | LEN | 见下 |
| CRC8 | 1 | 多项式 0x07，覆盖 LEN/TYPE/SEQ/PAYLOAD |

检测负载：`frame_id(1) + n(1) + n × [cls(1) conf(1) x(2) y(2) w(2) h(2)]`，小端。

---

## 6. 指标记录（做完填这里，再写进简历）

| 指标 | 测量方法 | 实测 |
|---|---|---|
| 串口链路 RTT | `tools/latency_test.py`，100 次取平均 | **avg 3.67 ms，p95 4.07 ms，0 丢包**（2026-11-17 实测） |
| 检测帧率 | `main.py` 实时预览（imgsz=320） | **10.1–10.3 fps**（屏幕实测） |
| 处理+发送耗时 | 每帧耗时（屏幕实测） | **18–19 ms/帧**（60 帧压测 avg 45.5 ms、p95 27.6 ms，含模型预热） |
| 连续运行 | 挂机跑 2 小时，看 CRC 错误/丢帧计数 | ___ 小时，错误 ___ 次 |
| 目标跟随误差（可选） | 舵机云台对着移动目标，目测/录像 | ±___° |

> 只写你真实测到的数字。没有数据就不写，千万别编。

---

## 7. 演示视频脚本（约 60 秒）

直接双击 `pc\record_demo.bat`（或 `python tools/record_window.py --duration 60 --output demo.mp4`），会录制 `Vision-Link` 窗口的完整画面（含检测框/FPS），录完在 `pc\demo.mp4`。

1. **0–10s**：镜头拍桌面，相机对着手/水杯/手机移动，PC 屏幕显示检测框；
2. **10–25s**：串口监视器窗口，展示 `AA 55 ...` 原始帧滚动；
3. **25–40s**：STM32 板子特写——OLED 上显示目标坐标，LED/蜂鸣器随目标出现动作；
4. **40–55s**：舵机云台跟着目标转（如果有舵机）；
5. **55–60s**：终端打印统计：`fps=xx, avg=xx ms, crc_err=0`。

---

## 8. 简历写法（做完再填数字）

> **Vision-Link：视觉自动构图云台**（个人项目，GitHub）
> - 基于 OpenCV + YOLOv8n 检测画面主体，设计"帧头 + 长度 + CRC8"自定义二进制协议，将主体中心坐标经 USART1 下发至 STM32F103；
> - STM32 端采用 **DMA + 空闲中断 + 环形缓冲 + 状态机** 解析协议，按目标中心偏差输出 PWM，控制舵机云台自动构图（带死区与低通滤波）；
> - 串口链路 RTT **avg 3.67 ms / 100 次 0 丢包**；PC 端实时处理 **18–19 ms/帧**（imgsz=320）。

---

## 9. 常见问题

**Q：OpenCV 打开的是笔记本摄像头，不是采集卡？**
A：DirectShow 顺序里 `0 = Integrated Webcam`、`1 = USB Video（采集卡）`、`2 = OBS Virtual Camera`。把 `config.yaml` 的 `source.index` 改成 `1`；同时**关闭 Windows 相机 App / 腾讯会议**，它们会独占采集卡。

**Q：检测框太大 / 框住整个人？**
A：YOLO 的 `person` 是整身框，人坐得近就占满画面，这是正常的。想要贴脸的框，把 `config.yaml` 的 `detector.backend` 改成 `face`（Haar 人脸）；或者把相机退远一点、换水杯/手机这种小目标演示。

**Q：采集卡在 OpenCV 里打不开 / 花屏？**
A：先用 `tools/list_cameras.py` 确认序号；Windows 下强制用 `cv2.CAP_DSHOW` 后端；1080p 建议设置 `fourcc=MJPG`。

**Q：串口收到乱码？**
A：确认波特率 115200、COM 口是**核心板 MicroUSB** 枚举出来的 CH340（不是板上 USB1）；确认共地。

**Q：STM32 收不到数据？**
A：先短接 PA9/PA10 做回环测试；确认 USART1 的 DMA 是 Circular 模式、NVIC 里 USART1 中断已开。

**Q：舵机抖动？**
A：舵机电源独立 5V、和板子共地；PWM 限幅 500–2500us；控制周期 20ms，加一阶低通。
