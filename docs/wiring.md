# 接线说明

## WonderCam ↔ STM32（I2C）

WonderCam 对外为 4 针 I2C 接口（默认器件地址 **0x32**）。按信号连接：

| WonderCam (I2C) | 接到 STM32（洋桃1号） |
|---|---|
| SCL | **PB0** |
| SDA | **PB1** |
| GND | **GND**（务必共地） |
| 5V  | 5V（若 WonderCam 已用 USB 供电，此脚可不接） |

> 说明：模块 4 针插头的物理线序以实测/丝印为准。本机实测为「5V / GND / SDA / SCL」，
> 其中 5V 保持悬空（由 USB 供电），GND 接板 GND，SDA→PB1，SCL→PB0。
> 因 SCL/SDA 接反不会损坏器件，若通信不通可对调这两根再试。

## 舵机 ↔ STM32

| 舵机线 | 接到 |
|---|---|
| 信号（橙/黄） | **PA15**（TIM2_CH1，部分重映射1） |
| 电源（红） | 洋桃板舵机接口的 5V |
| 地（棕/黑） | 洋桃板舵机接口的 GND |

> 舵机电流较大，务必由开发板 5V 提供足够供电，避免与 MCU 抢电导致复位。

## 供电

- WonderCam：USB 供电（5V，典型约 300mA）。
- 洋桃1号 / STM32：核心板 MicroUSB（程序下载 + USART1 + 供电）。

## 注意事项

- 共地是 I2C 正常工作的前提，WonderCam 的 GND 必须与 STM32 的 GND 相连。
- PA15 属于 JTAG 的 JTDI，固件中已通过 `AFIO->MAPR` 关闭 JTAG（保留 SWD）后复用为 TIM2_CH1。
