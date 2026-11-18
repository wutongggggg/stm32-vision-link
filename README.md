# STM32 + WonderCam(K210) 边缘视觉舵机跟随

基于幻尔 WonderCam（K210 视觉模块）与 STM32F103C8T6 的端侧视觉闭环：
WonderCam 负责摄像头采集与颜色识别，通过 **I2C** 输出目标框坐标；STM32 作为 I2C 主机读取结果，
计算目标中心偏差并输出 **50 Hz PWM** 驱动舵机跟随目标。整个链路不依赖 PC，属于真正的端侧
“视觉 → 决策 → 执行”闭环。

## 系统架构

```
        OV2640 摄像头
             │
     ┌───────┴────────┐
     │  WonderCam(K210)│   颜色识别 / 目标框(x,y,w,h)
     │   I2C Slave 0x32│
     └───────┬────────┘
             │ I2C (SCL/SDA/GND/5V)
     ┌───────┴──────────┐
     │ STM32F103C8T6     │  软件 I2C 主机 + 控制算法
     │ (洋桃1号开发板)    │  TIM2_CH1 → PA15
     └───────┬──────────┘
             │ 50 Hz PWM (500~2400 us)
          舵机
```

## 硬件

| 部件 | 说明 |
|---|---|
| 主控板 | 洋桃1号开发板（YoungTalk STM32F103C8T6 核心板，72MHz，64KB Flash / 20KB RAM） |
| 视觉模块 | 幻尔 WonderCam（Kendryte K210 + OV2640 200 万像素，内置颜色识别等 AI 功能） |
| 执行器 | 舵机（SG90 / MG90 等，50Hz PWM 控制） |
| 接口 | WonderCam 的 4 针 I2C 输出；舵机信号接 PA15 |

接线见 [docs/wiring.md](docs/wiring.md)，I2C 寄存器与通信协议见 [docs/protocol.md](docs/protocol.md)。

## 关键实现

### 1. 软件 I2C 主机（PB0 = SCL，PB1 = SDA）
用 GPIO 开漏模拟 I2C，实现起始 / 停止 / 读字节 / 写字节，以及 16 位寄存器读取。
实测 WonderCam 的 16 位寄存器地址为**低字节先**（version 寄存器读出 `v0.7.4` 可作校验）。

### 2. 读取颜色识别结果（器件地址 0x32）
- `0x1000` 结果刷新（读 1 字节）
- `0x1001` 识别到的目标数量
- `0x1002` 第一个目标颜色 ID
- `0x1030 ~ 0x1037` 第一个目标 `x / y / w / h`（各 16 位，低字节在前；坐标范围 320×240）

由 `x、w` 计算目标中心 `CX = x + w/2`（画面水平中线为 160）。

### 3. 舵机 PWM（TIM2_CH1 → PA15）
- PA15 默认是 JTAG 的 JTDI，需通过 `AFIO->MAPR` 的 `SWJ_CFG` **禁用 JTAG（保留 SWD）** 才能释放给 TIM2。
- `AFIO->MAPR` 设置 TIM2 **部分重映射1**，CH1 → PA15。
- 定时器：PSC=7、ARR=19999 → 1us 计数、20ms 周期（50Hz）；CCR 直接写脉宽 500~2400us。

### 4. 跟随控制
```
err    = CX - 160                      // 目标中心偏差
deadzone: |err| < 15 → 视为 0
target = 1500 + err * 5 (us/px)，限幅 600~2400
cur   += (target - cur) * 0.25         // 低通平滑
CCR   = cur
```
物体偏左舵机往左、偏右往右，连续平滑跟随；上电时先做一次 1000→2000→1500 的扫动自检。

## 目录结构

```
.
├── README.md
├── platformio.ini
├── src/
│   └── main.c              # 主程序：软件I2C + 读坐标 + 舵机PWM + 跟随控制
└── docs/
    ├── wiring.md           # 接线说明
    └── protocol.md         # I2C 通信协议与寄存器表
```

## 编译与烧录

- 编译：PlatformIO（`board = genericSTM32F103C8`，`framework = stm32cube`）
  ```bash
  pio run
  ```
- 烧录：串口 ISP（FlyMcu，选 CH340 对应 COM，115200）；或 ST-Link / 其它调试器。
- 运行：WonderCam 通过 USB 供电并在其屏幕/按键选择「颜色识别」并学习目标颜色；
  STM32 上电后先自检扫动，之后即可跟随。

## 结果

红色物体在镜头前左右平移时，舵机连续同向跟随；串口实时输出 `CX / err / pulse`，
`pulse` 在 600~2400us 之间随目标位置变化。
