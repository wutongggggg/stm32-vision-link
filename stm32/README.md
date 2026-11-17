# STM32 端集成说明（Keil MDK + STM32CubeMX）

> **前置提醒**：本工程需要 **Keil MDK-ARM 5**（ARM 编译器）。如果电脑上的 Keil 是 **C51 版**
> （目录里只有 `C:\Keil_v5\C51`、用来编 STC 单片机），它是编不了 STM32 的，需要另外安装
> MDK-ARM；两者可以装在同一目录共存。

目标芯片：**STM32F103C8T6**（洋桃1号核心板，FlyMcu 实测 64KB Flash / 20KB RAM）。

---

## 0. 方式 A：PlatformIO（推荐，电脑上已装好）

不用 Keil、不用 CubeMX、不用 ST-Link。VSCode 装好 PlatformIO 插件后，直接打开 `stm32` 文件夹即可。

**编译：**

```bash
pio run
```

**串口下载（核心板 MicroUSB + ASP 自动下载）：**

1. USB 线插 **核心板**上的 MicroUSB（蓝色小板右侧那个口，不是开发板上的 USB1）；
2. 确认 ASP 功能开启：黄色 ASP 指示灯**常亮**表示开启，**暗光**表示关闭；双击 **MODE** 键可开关 ASP（单击 MODE 是总电源，长按切换 FLASH-ISP / RAM-ISP）；
3. 执行（COM 口按设备管理器实际改）：
   ```bash
   pio run -t upload --upload-port COM3
   ```
   如果 PlatformIO 的 `stm32flash` 上传不触发 ASP，用 **FlyMcu** 打开 `build_out/firmware.hex` 下载（洋桃官方推荐，最稳）。
4. 下载完成后程序会自动复位运行；如果没运行，按一下复位键。

**看串口输出：**

```bash
pio device monitor -b 115200
```

> 首次编译已在本机验证通过：**RAM 1356 B / 20 KB（6.6%），Flash 9444 B / 64 KB（14.4%）**。
> 已导出 `build_out/firmware.hex` 和 `firmware.bin`，可直接用 **FlyMcu** 串口下载（核心板 MicroUSB，ASP 自动下载）。
> 如果 VSCode 里 PlatformIO 找不到板子，先在终端执行 `pio device list` 确认 COM 口。

**VSCode 里操作**：打开 `stm32` 文件夹 → 左侧 PlatformIO 图标 → `Project Tasks → genericSTM32F103C8 → Build / Upload / Monitor`。
也可以直接按 `Ctrl+Shift+B`，选 **PlatformIO: Build**（`.vscode/tasks.json` 已配置好，不依赖插件也能编译）。

---

## 方式 B：Keil MDK + STM32CubeMX（传统方式，可选）

## 1. CubeMX 新建工程

1. `File → New Project`，搜索 `STM32F103C8Tx`，选中 LQFP48 封装；
2. **RCC**：`High Speed Clock (HSE)` 选 `Crystal/Ceramic Resonator`；
3. **SYS**：`Debug` 选 `Serial Wire`；
4. **Clock Configuration**：
   - HSE = 8 MHz
   - PLL MUL = ×9 → **SYSCLK = 72 MHz**
   - AHB = 72 MHz，APB1 = 36 MHz，APB2 = 72 MHz
5. **USART1**：
   - Mode: `Asynchronous`
   - Baud Rate: `115200`，8N1，无流控
   - `DMA Settings` → Add → `USART1_RX`，Mode = **Circular**，Data Width = Byte
   - `NVIC Settings` → 勾选 `USART1 global interrupt`
6. **TIM2**（用舵机才需要）：
   - Clock Source: `Internal Clock`
   - Channel1: `PWM Generation CH1`
   - Parameter Settings: Prescaler = `71`，Counter Period = `19999`
   - 生成后由 `servo_pwm.c` 里的重映射代码接管 PA15
7. **Project Manager**：
   - Toolchain / IDE = `MDK-ARM`，Min Version V5
   - Code Generator: 勾选 `Generate peripheral initialization as a pair of .c/.h files`
8. `GENERATE CODE`，用 Keil 打开生成的工程。

> 如果 CubeMX 里 PA15 不能直接选成 TIM2_CH1，不用管——保持 TIM2 CH1 默认引脚即可，`servo_pwm.c` 会自己把 PA15 重映射成 TIM2_CH1。

## 2. 把源文件加入 Keil

在 Keil 工程里新建两个 Group（或加到已有 Group）：

- `VisionLink/Inc`：`protocol.h`、`crc8.h`、`ringbuf.h`、`app_vision_link.h`、`servo_pwm.h`
- `VisionLink/Src`：`protocol.c`、`crc8.c`、`ringbuf.c`、`app_vision_link.c`、`servo_pwm.c`

`Options for Target → C/C++ → Include Paths` 加上 `..\Inc`（按实际路径）。

> **不要**把 `integration_example.c` 加进工程——它整段是注释。

## 3. 改两处代码

`main.c`：

```c
/* USER CODE BEGIN Includes */
#include "app_vision_link.h"
#include "servo_pwm.h"
/* USER CODE END Includes */

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_DMA_Init();              /* 必须在 MX_USART1_UART_Init 之前 */
  MX_USART1_UART_Init();

  /* USER CODE BEGIN 2 */
  VisionLink_Init(&huart1);
  Servo_Init();
  /* USER CODE END 2 */

  while (1)
  {
    /* USER CODE BEGIN 3 */
    VisionLink_Poll();
    /* USER CODE END 3 */
  }
}
```

`stm32f1xx_it.c`：

```c
void USART1_IRQHandler(void)
{
  /* USER CODE BEGIN USART1_IRQn 0 */
  VisionLink_OnUartIdle(&huart1);
  /* USER CODE END USART1_IRQn 0 */
  HAL_UART_IRQHandler(&huart1);
  /* USER CODE BEGIN USART1_IRQn 1 */
  /* USER CODE END USART1_IRQn 1 */
}
```

应用回调（写在 `main.c` 末尾或新建 `app.c`）：

```c
void VisionLink_OnDetection(const proto_det_t *det)
{
    if (det->n == 0u) {
        Servo_SetAngle(90.0f);
        return;
    }
    uint16_t cx = (uint16_t)(det->targets[0].x + det->targets[0].w / 2u);
    float angle = 60.0f + (float)cx * 60.0f / 1280.0f;
    Servo_SetAngle(angle);
}
```

## 4. 下载方式

两种都行：

| 方式 | 需要 | 优点 | 缺点 |
|---|---|---|---|
| **ST-Link V2（推荐）** | 一个 ST-Link 小 U 盘（20 元左右） | 能下载 + 断点调试 | 要买 |
| 串口 ISP | **核心板 MicroUSB**（CH340）+ FlyMcu | 零成本，洋桃自带 ASP 自动下载 | 只能下载，不能调试 |

串口 ISP 步骤：USB 线插 **核心板 MicroUSB** → 确认 ASP 黄灯常亮（暗光就双击 MODE 开启）→ FlyMcu 选 COM 口、115200、下载 `build_out/firmware.hex` → 完成后自动复位运行。

> 强烈建议花 20 块买个 ST-Link：调协议时能打断点看 `s_frame`、`s_rb`，效率差好几倍。

## 5. 自测顺序

1. 只烧 `main.c` 空循环 + `VisionLink_Init`，用 PC 的 `tools/latency_test.py` 测 PING/PONG，确认串口通；
2. 再烧 DET 解析，在回调里把 `det->n` 打到 OLED 或点亮 LED；
3. 最后接舵机做跟随。

## 6. 资源占用参考

| 模块 | RAM | Flash |
|---|---|---|
| 协议解析 + 环形缓冲 | < 1.5 KB | ~3 KB |
| 舵机 PWM | < 0.2 KB | ~1 KB |

20KB RAM 完全够用，剩下来还能跑 OLED 显存（一般 1KB 左右）。
