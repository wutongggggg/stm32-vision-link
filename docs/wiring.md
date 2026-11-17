# 接线与引脚（洋桃1号 / STM32F103C8T6）

## 1. 串口（必须）

**用核心板上的 MicroUSB**（蓝色核心板右侧那个口）接电脑，不是开发板上的 USB1。核心板通过 CH340 接 USART1：

| STM32 | 核心板 CH340 | 说明 |
|---|---|---|
| PA9 | TX → RX | USART1_TX |
| PA10 | RX ← TX | USART1_RX |
| GND | GND | 共地 |

- 核心板 MicroUSB：**支持程序下载 + USART1 通信 + 供电**；
- 开发板底部 **USB1：只能供电，不支持程序下载**（官方说明书明确写的，别插错）；
- USB2（USB-A）是 U 盘接口，也不是串口。

插好后 Windows 设备管理器 → **端口 (COM 和 LPT)** 会出现 `USB-SERIAL CH340 (COMx)`。如果没有任何新设备，先换一根**确定能传数据**的 MicroUSB 线。

> 洋桃1号支持 **ASP 自动下载**：核心板黄色 ASP 灯**常亮**=开启，**暗光**=关闭；双击 **MODE** 键切换 ASP，单击 MODE 是总电源开关，长按切换 FLASH-ISP / RAM-ISP。

> 如果设备管理器显示未知设备：装 CH340 驱动（洋桃资料包自带，或 wch.cn 官网）。

## 2. 舵机（可选）

板载 **P7 舵机接口**，丝印 `GND / 5V / PA15`：

| 舵机线 | 接 P7 |
|---|---|
| 棕/黑（GND） | GND |
| 红（VCC） | 5V（建议外部 5V 电源，和板子共地） |
| 橙/黄（信号） | PA15 |

PA15 做 PWM 需要：
- `__HAL_AFIO_REMAP_SWJ_NOJTAG()`（关 JTAG、保留 SWD，否则 PA15 是 JTDI）
- `__HAL_AFIO_REMAP_TIM2_ENABLE()`（TIM2 完全重映射，CH1 → PA15）
- TIM2：PSC=71（72MHz/72=1MHz），ARR=19999（50Hz），CCR=500–2500（0.5–2.5ms）

代码见 `stm32/Src/servo_pwm.c`。

## 3. 板载外设（无需接线）

| 外设 | 位置/接口 | 本项目用途 |
|---|---|---|
| 0.96" OLED | P12，I2C（PB6/PB7，P11 I2C总线） | 显示目标数量/坐标/状态 |
| 蜂鸣器 | 板载 | 目标出现报警 |
| LED1/LED2 | 核心板 | 状态指示 |
| 按键 KEY1/KEY2/MODE | 核心板 | 启停/切换模式 |
| 8 位数码管 | P9 | 可显示帧计数（可选） |

> OLED/蜂鸣器/LED 的具体驱动引脚以洋桃1号原理图为准，直接用洋桃资料包里的例程移植最快。

## 4. 相机链路

```
相机 HDMI OUT ──HDMI线──> 绿联采集卡 ──USB-A──> PC
```

- 采集卡是 UVC 免驱设备，Windows 里就是 "USB Video"；笔记本自带摄像头是 "Integrated Webcam"。
- 用 `pc/tools/list_cameras.py` 看名字和序号：**0 = Integrated Webcam，1 = USB Video（采集卡）**。
- 相机 HDMI 输出建议 1080p24、8-bit 4:2:2，关闭 HDMI 信息叠加。
- 1080p30 建议在 OpenCV 里设置 `CAP_PROP_FOURCC = MJPG`，否则默认 YUY2 可能只有 5–10fps。

## 5. 电源

- 整板用 USB 5V 供电即可。
- 接舵机时，舵机电流可能到 500mA–1A，**别从板子 5V 取电**，用独立 5V 电源并共地。
