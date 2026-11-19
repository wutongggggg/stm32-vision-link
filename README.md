# Vision-Link：视觉自动构图云台（v1 → v2 两代迭代）

同一个项目、同一条技术路线，记录从「电脑做视觉」到「端侧 K210 做视觉」的迭代过程。

```
stm32-vision-link/
├─ v1_pc_yolo_stm32/         第一代：相机 + 电脑(OpenCV + YOLOv8n) + STM32
└─ v2_k210_stm32/            第二代：K210(WonderCam) + STM32（端侧，无需电脑）
```

## v1：相机 + 电脑 YOLO + STM32（v1_pc_yolo_stm32）
- 相机 HDMI → USB 采集卡 → 电脑
- PC 端 OpenCV + YOLOv8n 检测主体，计算目标中心
- 自定义二进制协议（帧头 AA55 + 长度 + 类型 + 序号 + CRC8）经 USB 串口(115200) 下发 STM32
- STM32 USART1 + DMA + 空闲中断 + 环形缓冲 + 状态机解析；按偏差输出 50Hz PWM 驱动舵机云台（±20px 死区 + 低通）
- 实测：串口 PING/PONG 100 次零丢包，平均 RTT 3.67 ms
- 目录：`pc/`（Python 端）、`stm32/`（固件）、`docs/`（协议/接线/演示）

## v2：K210 + STM32（v2_k210_stm32）
- 幻尔 WonderCam（Kendryte K210 + OV2640）内置颜色识别，端侧完成「看」
- STM32 以软件 I2C（PB0=SCL / PB1=SDA，器件地址 0x32）读取目标框 x/y/w/h（16 位寄存器、低字节先）
- STM32 计算中心偏差，输出 50Hz PWM（TIM2_CH1→PA15，禁用 JTAG 释放复用）驱动舵机；±15px 死区 + 低通
- 整机脱离电脑与采集卡，形成真正的端侧「视觉→决策→执行」闭环
- 目录：`platformio.ini`、`src/main.c`、`docs/`（接线/协议）

## 代际关系
- v1 把重计算放在电脑、MCU 做实时控制；
- v2 把感知搬到 K210 端侧、MCU 只做 I2C 读取与执行；
- 两代共用同一套「视觉坐标 → 云台控制」思想，v2 是 v1 的端侧化升级。
