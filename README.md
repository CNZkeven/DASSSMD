# Dual-axis-single-stroke-STM32-motor-drive
##  STM32F407 二轴步进电机点动与一笔画控制器

本固件运行在 µC/OS-III 上，保留板载按键点动，并新增触摸屏一笔画：操作者在屏幕上确认人工中心，单指画一笔，预览后让两轴按 DDA 路径播放。点动和播放都以“停机优先”为原则，播放期间不会接受按键点动。

| 项目 | 当前实现 |
| --- | --- |
| MCU | STM32F407ZGT6，HSE 25 MHz，SYSCLK 168 MHz |
| RTOS | µC/OS-III，1000 Hz tick，Motor/UI 两个应用任务 |
| 显示 | NT35510 480×800，FSMC 并口，RGB565 |
| 输入 | GT9xx 电容触摸 + 板载 KEY1/KEY2 |
| 电机 | X/Y 两路 STEP/DIR；TIM2 双通道脉冲输出 |
| 路径 | `User/plotter/plotter_path.c`：触摸映射、路径状态、DDA 插补 |
| 外设库 | STM32F4xx Standard Peripheral Library，不是 HAL |

## 目录

1. [快速开始](#1-快速开始)
2. [系统架构](#2-系统架构)
3. [交互流程](#3-交互流程)
4. [路径与运动约束](#4-路径与运动约束)
5. [硬件接口与安全](#5-硬件接口与安全)
6. [代码结构与接口](#6-代码结构与接口)
7. [测试与构建](#7-测试与构建)
8. [已知限制与未验证项](#8-已知限制与未验证项)
9. [排查与诊断](#9-排查与诊断)

## 1. 快速开始

构建脚本没有可执行位；构建和烧录均可用 `bash` 调用：

```bash
bash build.sh
PYTHONDONTWRITEBYTECODE=1 pytest -p no:cacheprovider -q
```

构建会在 `build/` 下生成：

- `zhsx.elf`：链接后的固件
- `zhsx.hex`：Intel HEX 镜像
- `zhsx.bin`：裸二进制镜像
- CMake linker memory report：Flash/RAM 使用量

烧录仍使用：

```bash
bash flash.sh
```

烧录前确认 `openocd.cfg` 保留 `connect_assert_srst`。

**烧录成功与否只看 OpenOCD 输出里的 `verified N bytes`，不要看退出码**（`flash.sh` 已按这个判据写）：

- 以 `shutdown` 结尾时 OpenOCD 恒返回 0，退出码不能区分成功失败；
- `reset run` 之后必然报 `Error connecting DP: cannot read IDR`——因为固件已经在跑，PA14 把 SWCLK 占了。这是预期现象，不是故障。

## 2. 系统架构

### 2.1 任务

```text
main()
 └─ OSInit() → AppTaskStart(prio 2) → OSStart()
      ├─ CPU_Init / 时钟 / SysTick / Debug USART
      └─ AppTaskCreate()
           ├─ Motor_Task prio 3：10 ms，按键点动与播放仲裁
           └─ UI_Task    prio 4：电机页 50 ms；作画/播放页 10 ms，LCD/触摸、页面状态和路径交互
```

| 任务 | 周期 | 职责 |
| --- | --- | --- |
| `Motor_Task` | 10 ms | 扫描按键；处理点动状态机；接受/停止一笔画播放请求；维护运动互斥 |
| `UI_Task` | 电机页 50 ms；作画/播放页 10 ms | 初始化 LCD/触摸；驱动 `DRAW → CENTER READY → ...` 流程；刷新预览、进度和结果 |
| `TIM2_IRQHandler` | 1 kHz 更新事件 | 点动时统计步数；播放时执行 DDA 时间片并写双轴比较值 |

Motor 任务数值优先级小于 UI 任务，因此松键停机优先于界面刷新。UI 任务只通过电机模块接口表达请求，不直接改定时器寄存器。

优先级不要排到 1 或 2：那是 uCOS 内核的 tick 任务和启动任务。数值见 `User/app_cfg.h`，那里也记录了这条优先级顺序的理由。

### 2.2 模块关系

```text
             ┌────────────────┐
             │    UI_Task     │
             │ touch + pages  │
             └───────┬────────┘
                     │ path capture / playback request
          ┌──────────▼──────────┐
          │ plotter_path core   │
          │ map + state + DDA   │
          └──────────┬──────────┘
                     │ DDA mask + direction
             ┌───────▼────────┐
             │   Motor_Task   │
             │ jog arbitration│
             └───────┬────────┘
                     │
             ┌───────▼────────┐
             │   bsp_motor    │── TIM2 CH2/CH3 + DIR
             └────────────────┘
```

### 2.3 所有权与状态分层

四层各有独立状态，靠显式契约衔接。**理解这张表比读任何单个文件都重要**：

```text
UI_Task ──发布冻结路径指针──▶ Motor_Task ──占用定时器──▶ ISR 引擎 ──▶ TIM2 CCR
（页面状态机）               （请求生命周期）            （脉冲调度）
```

| 层 | 状态变量 | 定义 | 取值语义 |
| --- | --- | --- | --- |
| UI | `s_ui_state` | `UI_State_t`，`ui_task.c` | 9 个页面/交互阶段 |
| Motor 任务 | `s_plot_task_state` | `MotorPlotState_t`，`motor_task.c` | `IDLE/PENDING/RUNNING/DONE/ABORTED/REJECTED` |
| Motor 任务 | `s_state` | 内部 `JogState_t` | `JOG_IDLE / JOG_DIR_SETUP / JOG_RUNNING` |
| ISR 引擎 | `s_plot_state` | `MotorPlotEngineState_t`，`bsp_motor.c` | `IDLE/RUNNING/STOPPING/DONE/ABORTED` |

四条硬边界：

- **UI 从不碰定时器寄存器。** 表达诉求只用 `Motor_PlotterRequestStart()` / `Motor_PlotterRequestStop()`。
- **路径所有权。** UI 持有静态的 `PlotterPath_t`，Motor 任务是唯一消费者；只有发布终态后才释放借用的指针（`Plotter_PublishTerminal()`），保证回放期间路径不会在脚下被改写。
- **点动互斥。** `Motor_PlotterOwnsTimer()` 使 `Motor_StartPulse/StopPulse/SetDir/ResetSteps` 在引擎 `RUNNING/STOPPING` 时全部空转返回。播放拥有定时器和轴控制权。
- **紧急路径例外。** `Motor_StopAll()` 显式绕过上述所有权守卫直写 `CCR=0`，专供 fault 处理调用（见 §5.3）。

**跨层竞态用临界区收口。** `PENDING → RUNNING` 的提交在临界区内完成，所以一个落在“快照之后、判定之前”的 `STOP` 不会被擦除——它会在发布终态时原子地降级成 `ABORTED`。引擎侧同理：只有在全部 task/ISR 共享状态就绪后才发布 `RUNNING`。引擎态一旦丢失或非法，一律判 `ABORTED`，**绝不回落到点动**。

## 3. 交互流程

### 3.1 一笔画主流程

```text
UI_PAGE_MOTOR ──DRAW──▶ CENTER_CONFIRM ──CENTER READY──▶ READY
                                                          │
                          ┌───────────────────────────────┤
                 DEMO 按下│                    画板内落指 │
                          ▼                               ▼
                    DEMO_MENU ──选中示例──▶ PREVIEW ◀──抬笔成功── CAPTURING
                          △                   │                    │
                          └───EXAMPLES────────┤                    │抬笔失败
                                              │                    ▼
                                              │                 INVALID
                          START               │                    │REDRAW
                            ▼                 │                    ▼
                         PLAYING              │               (回 READY)
                            │                 │
                     DONE / ABORTED           │
                            ▼                 │
                         RESULT ──BACK──▶ UI_PAGE_MOTOR
```

完整转移表（所有分支）:

| 状态 | 输入 | 结果 |
| --- | --- | --- |
| `UI_PAGE_MOTOR` | `DRAW` 按下 | → `CENTER_CONFIRM`；点动运行中或播放 `PENDING/RUNNING` 时**拒绝进入** |
| `UI_DRAW_CENTER_CONFIRM` | `CENTER READY` 按下 | → `READY`。该控件的 DOWN **只确认原点，绝不当成笔画首点** |
| | `BACK` 按下 | → `UI_PAGE_MOTOR` |
| `UI_DRAW_DEMO_MENU` | 选中某行 | 载入示例 → `PREVIEW`；`s_center_ready` 为假或路径已提交时忽略 |
| | `BACK` | 若已选过示例则清示例回 `READY`，否则直接回 `READY` |
| | `PREV` / `NEXT` | 翻页（3 项/页，共 3 页），重绘本页 |
| `UI_DRAW_READY` | 画板内 DOWN | `s_last_touch` 置为**画板中心**（非首触点）→ `CAPTURING`，并追加首点 |
| | `DEMO` 按下 | → `DEMO_MENU` |
| | `BACK` 按下 | → `UI_PAGE_MOTOR` |
| | `MULTI` / `ERROR` | → `INVALID`（`MULTI TOUCH` / `TOUCH ERROR`） |
| `UI_DRAW_CAPTURING` | `MOVE` | 追加采样点（越界夹到画板边界） |
| | `UP` | 成功 → `PREVIEW`；点数不足 → `INVALID`（`STROKE TOO SHORT`） |
| | `MULTI` / `ERROR` / `DOWN` | → `INVALID`（多指 / 触摸错误 / 意外按下） |
| `UI_DRAW_PREVIEW` | `START` | → `PLAYING`；请求被拒时留在本页显示 `START REQUEST REJECTED` |
| | `REDRAW` | 手绘：清路径回 `READY`；示例：回 `DEMO_MENU` |
| | `CANCEL` | 手绘：回电机页；示例：清示例回 `READY` |
| `UI_DRAW_INVALID` | `REDRAW` | → `READY`（重新采集） |
| | `CANCEL` | → `UI_PAGE_MOTOR` |
| `UI_DRAW_PLAYING` | `STOP` 按下 | 发停止请求，状态文字变 `STOP REQUESTED`，页面不变 |
| | 轮询到 `DONE` / `ABORTED` | → `RESULT` |
| | 轮询到 `REJECTED` | 未开始运动 → 退回 `PREVIEW` 可重试；已开始过长 → `RESULT` |
| `UI_DRAW_RESULT` | `BACK` 按下 | → `UI_PAGE_MOTOR`（同时清除中心确认） |

几点行为语义：

- `DRAW` 从电机点动页进入中心确认。
- `CENTER READY` 表示操作者确认当前机构位置为软件坐标中心。
- 画板正中心固定对应 `(0, 0)`，十字标记表示电机初始位。整个画板覆盖 X `±3500`、Y `±2300 steps`；触摸点按画板绝对位置映射，不再相对首触点平移。
- 一次触摸抬起后进入 `PREVIEW`；`REDRAW` 清除当前路径并重新采集，`CANCEL` 返回电机点动页。
- `START` 请求播放；播放页显示进度并提供 `STOP`。正常完成或停止后进入 `RESULT`。
- 若路径溢出、点数不足、多指或触摸读取错误，进入 `REJECTED`/无效页；此时不会启动电机，可以 `REDRAW` 重试或 `CANCEL` 返回。
- 当前没有抬笔机构，因此中心到首触点的连线也属于实际作画路径，并在采集和预览中显示。若不希望出现额外连线，请从画板中心起笔。

**触摸读取方式在两种页面下不同**，这是刻意的：

- 电机页走轮询（`GTP_GetTouchPoint()` + 200 ms 去抖），50 ms 一次，只处理点击语义。
- 作画/播放页走 ISR 置位的事件（`DOWN/MOVE/UP/MULTI/ERROR`），10 ms 一次，才能拿到连续轨迹。

GT9xx 中断只置一个标志位，**I2C 读写和全部 LCD 绘制都在 UI 任务里做**，不在中断路径上。

### 3.2 按键点动

| 情况 | 行为 |
| --- | --- |
| KEY1 长按 | 当前选中轴正转 |
| KEY2 长按 | 当前选中轴反转 |
| 松开或两键同时按住 | 停止 |
| 运行中切轴 | 拒绝切换，保持原运动仲裁 |
| 上电时按键已按住 | 必须先松开一次，之后才武装点动 |

起一次新的点动会先清零步数、摆好 DIR、等 `MOTOR_DIR_SETUP_TICKS`（2 ms）建立时间，再出脉冲。运行中途反向同样要先停脉冲、重设 DIR、重新等建立时间。两键同按视为“停”在任务层判定，UI 不参与。

播放期间点动互斥：播放拥有运动定时器和轴控制权，KEY1/KEY2 不会插入另一套脉冲；`STOP` 请求会让播放安全退出并停止两轴。播放结束**不等于**立刻恢复点动——必须两个键都消抖松开一次，之后新的一次按压才会变成点动命令，避免“播放结束时手还按着键”直接变成一段意外行程。

### 3.3 内置一笔矢量示例

`CENTER READY` 后，画板页的 `DEMO` 按钮打开三页示例菜单：繁花、航母、战斗机、原创全身站姿角色、龙、机械心脏、1933 帆船徽章和“相依”情侣侧脸。选择示例只载入预览；点击 `START` 才会运动。示例预览可回到 `EXAMPLES` 更换，或点 `READY` 返回手绘画板。帆船徽章依据用户提供的图案，情侣侧脸依据用户提供的线稿，均转换为可单次连续运行的线描版。

| 按钮标签 | 中文 | 点数 | X 范围 | Y 范围 |
| --- | --- | ---: | ---: | ---: |
| `FLOWER` | 繁花 | 1501 | ±2705 | ±1778 |
| `CARRIER` | 航母 | 268 | -2600 … +2580 | -820 … +1552 |
| `FIGHTER JET` | 战斗机 | 159 | ±2520 | -1510 … +1740 |
| `CHARACTER` | 原创角色 | 363 | ±2444 | -1422 … +1521 |
| `DRAGON` | 龙 | 338 | -1800 … +2600 | -540 … +1740 |
| `HEART GEAR` | 机械心脏 | 467 | ±1950 | -1550 … +1774 |
| `1933 BADGE` | 帆船徽章 | 499 | ±1770 | ±1770 |
| `TOGETHER` | 相依 | 1479 | -1575 … +1617 | -1729 … +1008 |

所有示例均为单一路径，显式包含形状间的连接线，从中心 `(0,0)` 起笔并回到中心；点数不超过 2048，路径范围不超过 X `±2800`、Y `±1840 steps`。原创角色横向伸展站姿，以适应画板形状。

这三条不变量由 `tools/generate_plotter_demos.py`（生成时断言）和 `tests/test_plotter_demo.py`（读取固件点表与 SVG 双向核对）共同强制，改示例时会被测试挡住。

SVG 原图位于 `assets/one-stroke-*.svg`，离线生成器为 `tools/generate_plotter_demos.py`，固件矢量表位于 `User/plotter/plotter_*_points.inc`。固件只复制离线生成的整型点表，不在 MCU 上解析 SVG 或运行曲线计算。

## 4. 路径与运动约束

### 4.1 映射和软件限位

`plotter_path.h` 定义相对人工确认中心的软限位：

| 轴 | 允许范围 | 含义 |
| --- | ---: | --- |
| X | `-3500 ... +3500` steps | 相对确认中心的软件坐标，不是绝对位置测量 |
| Y | `-2300 ... +2300` steps | 相对确认中心的软件坐标，不是绝对位置测量 |

画板边缘（含边框像素）对应上述完整范围：左/右为 X `-3500/+3500`，上/下为 Y `+2300/-2300`，四角同时对应两轴边界。绘制中拖出画板的触点夹到最近边界，返回画板后继续采集，不会使整笔无效；画板外的初次按下不启动笔画。软件限位不是物理限位，仍须先人工将机构置于中心。最大采样点数为 `PLOTTER_MAX_POINTS = 2048`，超出后为 `PLOTTER_PATH_OVERFLOW`。播放默认主步频配置为 `PLOTTER_STEP_RATE_HZ = 1000` steps/s（每轴不超过 1000 steps/s），在上板确认驱动器时序和电机不失步前不代表已验证的安全速度。

映射是**绝对**的：画板正中心恒对应 `(0,0)`，不随首触点平移。采集时用对称四舍五入把像素偏移线性换算成步数，Y 轴取反（屏幕向下为正 → 机构向上为负）。

### 4.2 DDA 播放

`PlotterDda_Next()` 对每个线段产生一个主步时间片和 X/Y 方向。一个时间片可以同时返回 `PLOTTER_AXIS_X | PLOTTER_AXIS_Y`，因此两轴可在同一主步事件各发出一次脉冲。

电机层把 DDA 的轴步进掩码转换为 TIM2 CH2/CH3 的比较值和 DIR 电平。调度器整个跑在 1 kHz 的 TIM2 更新中断里（`Motor_PlotterScheduleISR()`），四个关键机制：

**① 双层流水线。** 维护两份掩码：`s_plot_active_mask`（本周期正在出脉冲的轴）与 `s_plot_preload_mask`（下周期的）。因为 CCR 使能了预装载，本周期写下的比较值要到下个更新事件才生效——所以**计数用的必须是“已经出完的那个周期”的掩码**，否则进度和停机判定会提前一个周期。

**② 速率分频。** `s_plot_rate_phase` 计数器按 `1000 / PLOTTER_STEP_RATE_HZ` 分频。当前值 1000，即 1:1，每 tick 一个主步。⚠️ 这里是整数除法，**`PLOTTER_STEP_RATE_HZ` 必须 ≤ 1000 且最好是 1000 的因数**：设成 2000 会因为 `1000/2000 == 0` 而静默退化成 1 kHz（不是加速），设成 300 之类非因数会引入速率抖动。

**③ 换向插入 DIR 建立窗口。** 方向变化时不立即出脉冲，而是把该主步存进 `s_plot_pending_*`，等 `MOTOR_DIR_SETUP_TICKS` 个 tick 再放行。这解决了播放中途换向时 DIR 与 STEP 的时序竞争，和点动路径用同一个建立时间常量。

**④ 停机等在途脉冲走完。** 收到停止请求后进入 `STOPPING`，持续写 mask 0，直到 `s_plot_active_mask == 0`（最后一个在途脉冲完整走完）才落到终端态 `DONE`/`ABORTED`。这样不会截断脉冲。故障路径会先停两轴再处理。

**步数计数是隔离的**：ISR 末尾累加 `s_steps[]` 只认 `s_pulsing[]`（只有点动会设置它），所以**播放不会污染点动页的步数显示**。

进度由累计的主步数算：`Motor_PlotterEngineGetProgress()` 返回 `completed/total × 100`，引擎 `DONE` 时直接返回 100。

## 5. 硬件接口与安全

### 5.1 主要引脚

| 功能 | 引脚/资源 |
| --- | --- |
| X STEP / DIR | PA1 / PA15，TIM2_CH2 |
| Y STEP / DIR | PA2 / PA14，TIM2_CH3 |
| KEY1 / KEY2 | PA0 / PC13 |
| LCD | FSMC Bank1 NORSRAM3，命令 `0x68000000`，数据 `0x68000002` |
| 触摸 | SCL=PD7，SDA=PD3，RST=PD6，INT=PG8，软件 I2C |
| 调试串口 | USART1：PA9 TX / PA10 RX，115200 8N1 |

TIM2 挂在 APB1 上，APB1 = HCLK/4 = 42 MHz，定时器时钟因此是 84 MHz：`84 MHz / 84 / 1000 = 1 kHz`，STEP 高电平 500 µs。实现见 `User/motor/bsp_motor.h` 顶部的常量块。

### 5.2 PA14/SWCLK 连接注意

PA14 同时是 SWCLK，但固件把它用作 Y 轴 DIR；固件运行后 SWD 可能立即失联。因此 `openocd.cfg` 必须保留：

```tcl
reset_config srst_only srst_nogate connect_assert_srst
```

不要恢复 `reset_config none separate`。这是调试连接的硬件约束，不是运行时错误。

### 5.3 Fault 停机

`Fault_Indicate()` 进入死循环前必须调用 `Motor_StopAll()`：TIM2 是硬件 PWM，即使 CPU 不再调度任务，非零 CCR 仍可能继续产生脉冲。该停机调用只操作电机外设寄存器，不调用 uCOS 服务。

`Motor_StopAll()` 因此在 `Motor_Init()` 尚未跑过时直接返回——否则会访问还没开时钟的 TIM2，在 fault 上下文里再触发一次总线错误。

每个 fault 用不同的 LED 组合指示：HardFault = PF6，MemManage = PF7，BusFault = PF8，UsageFault = PF6+PF8。

## 6. 代码结构与接口

### 6.1 源文件地图

```text
CMakeLists.txt
README.md
User/
├── main.c                启动任务、任务创建、CPU 时间戳
├── stm32f4xx_it.c        Fault 指示、TIM2/GTP 中断外壳
├── app_cfg.h             任务优先级与栈大小（含理由）
├── boot_trace.h          上电进度标记，见 §9.1
├── motor/                bsp_motor.c/.h：TIM2、STEP/DIR、DDA 引擎（ISR 内）、DIR 引脚日志
├── task/                 motor_task.c/.h：点动状态机与播放生命周期仲裁
├── plotter/              plotter_path.c/.h：路径采集、软限位、DDA（纯逻辑，可主机单测）
│                         plotter_demo.c + 8 个 plotter_*_points.inc：内置矢量表
├── ui/                   ui_task.c/.h：页面状态机、触摸事件分发
│                         ui_draw.c/.h：绘制原语、布局常量、Bresenham 画线
├── touch/                gt9xx.c：GT9xx 驱动（5688/911/9147/9157 自动识别）
│                         bsp_i2c_touch.c：软件 I2C
├── lcd/                  bsp_nt35510_lcd.c：NT35510 FSMC 驱动
├── key/                  bsp_key.c：消抖与上电武装
├── usart/                bsp_usart.c：USART1 调试输出
├── font/                 fonts.c：厂商中文字库（见下方说明）
└── flash/                bsp_spi_flash.c：字库编译依赖
```

`User/flash/bsp_spi_flash.c` 是野火中文字库 `fonts.c` 在 `GBKCODE_FLASH=1` 时 include 的驱动。电机界面只用英文字库、永远不会真的调它，但少了它 `fonts.c` 编不过——所以它留在 `${USER_SOURCES}` 里。

LCD 用的是英文字库，因此**屏上所有文字必须是 ASCII**；`UI_DrawStringBounded()` 会按 `max_width` 截断，不会越界写到屏幕外。

页面状态机的 9 个取值和全部布局常量分别集中在 `ui_task.h` 与 `ui_draw.h`，改版面只动 `ui_draw.h`。CANVAS 相关的 `UI_PLOT_*` 常量必须与 `plotter_path.h` 的软限位保持同一比例，调整时两处一起改。

### 6.2 核心接口

```c
/* User/plotter/plotter_path.h */
void PlotterPath_Begin(PlotterPath_t *, const PlotterCanvas_t *);
PlotterPathState_t PlotterPath_AppendTouch(PlotterPath_t *, uint16_t x, uint16_t y);
PlotterPathState_t PlotterPath_Finish(PlotterPath_t *);
void PlotterDda_Begin(PlotterDda_t *, const PlotterPath_t *);
uint8_t PlotterDda_Next(PlotterDda_t *, uint8_t *, int8_t *, int8_t *);

/* User/motor/bsp_motor.h */
void Motor_Init(void);
void Motor_StartPulse(MotorAxis_t axis);
void Motor_StopPulse(MotorAxis_t axis);
void Motor_StopAll(void);
void Motor_UpdateISR(void);

/* User/task/motor_task.h —— UI 唯一的运动请求入口 */
uint8_t         Motor_PlotterRequestStart(const PlotterPath_t *path);
void            Motor_PlotterRequestStop(void);
MotorPlotState_t Motor_PlotterGetState(void);
uint16_t        Motor_PlotterGetProgress(void);
```

`plotter_path.c` 不依赖任何 STM32 头文件，只有 `<stdint.h>`——这正是它能在主机上编译并单测的原因，改动时请保持这个性质。

CMake 的固件目标只由 `${USER_SOURCES}`、`${CMSIS_SOURCES}`、`${PERIPH_SOURCES}`、`${UCOS_SOURCES}` 四个活动源列表组成；USB 演示库不进入该目标。`User/plotter` 同时加入 include 路径和 `USER_SOURCES`。**新增源文件必须加进 `USER_SOURCES`**，否则不会参与链接。

## 7. 测试与构建

### 7.1 构建与烧录

交叉编译使用项目本地 STM32 GNU 工具链：

```text
/home/kevenchow/.local/share/stm32cube/bundles/gnu-tools-for-stm32/14.3.1+st.2/bin/arm-none-eabi-gcc
```

构建命令必须是 `bash build.sh`，不能用 host 编译替代固件链接验收。成功条件是同时得到 `build/zhsx.elf`、`build/zhsx.hex`、`build/zhsx.bin`，并在链接输出中看到 memory report。

### 7.2 主机端测试

```bash
PYTHONDONTWRITEBYTECODE=1 pytest -p no:cacheprovider -q
```

这些测试**不是靠搜源码字符串过关的**：它们从生产 `.c` 文件里**逐字抽取真实函数体**，只替换掉 I2C/LCD/定时器这些外部边界，然后在主机上编译执行。所以能验证真实的分支行为，而不只是“源码里有没有这句话”。

| 文件 | 覆盖什么 |
| --- | --- |
| `plotter_path_host.c` | C 主机测试桩：DDA 遍历、映射。被 `test_plotter_path.py` 以 `-Wall -Wextra -Werror` 编译 |
| `test_plotter_path.py` | 编译并运行上面那个桩；`-Werror` 保证路径核心无警告 |
| `test_motor_scheduler_host.py` | 用真实的 ISR 调度器 + DDA 跑在「CCR 预装载模型」上，验证双层流水线时序；不接触真实硬件 |
| `test_drawing_ui_host.py` | 执行真实 UI 帮助函数与路径核心，替换 LCD/电机边界；验证命令准入、路径不可变性与触摸状态转移 |
| `test_touch_events_host.py` | 用生产 GT9xx 解析器，仅替换 I2C 总线；覆盖新报文、总线失败、ACK |
| `test_canvas_render_host.py` | 画板渲染辅助函数与反向像素映射 |
| `test_plotter_demo.py` | 8 个示例的连续性/包络/回到中心；固件点表与 `assets/*.svg` 双向数值核对 |
| `test_static_contracts.py` | 源码级静态契约，覆盖难以在无硬件条件下触发的模式，直接对应 `error.md` 里的历史 bug；其中也包含本文档的数值契约 |

单独跑某一个：

```bash
PYTHONDONTWRITEBYTECODE=1 pytest -p no:cacheprovider -q tests/test_plotter_path.py
PYTHONDONTWRITEBYTECODE=1 pytest -p no:cacheprovider -q tests/test_static_contracts.py
```

### 7.3 文档与工具索引

| 路径 | 内容 |
| --- | --- |
| `error.md` | 综合 bug 修复记录（31 条），静态契约测试的由来 |
| `一笔画.md` | 示例图创作工作流：拓扑约束、走序设计、坐标保护、上板前核对清单 |
| `docs/superpowers/specs/` | 设计文档：F407 集成、温度仿真、UART GUI、一笔画、居中全幅画板 |
| `docs/superpowers/plans/` | 对应的实施计划 |
| `tools/generate_plotter_demos.py` | 离线把 SVG 转成固件点表（`.inc`），生成时断言全部不变量 |
| `assets/one-stroke-*.svg` | 8 个示例的一笔连线源图 |
| `.superpowers/sdd/` | 一笔画任务的简报/报告/评审/回归记录，其中 `task-4-pre-fix/` 是整份旧源码快照 |

`.superpowers/sdd/.../task-4-pre-fix/` 是历史存档（记录旧版「首触点作原点 + 越界即整笔作废」到现版「绝对画板映射 + 边界钳制」的对比），保留有据，但它会让全文检索和代码索引出现重影，**搜代码时请排除 `.superpowers/`**。

## 8. 已知限制与未验证项

以下内容必须上板、接实际驱动器并用示波器/量具验证：

- X/Y 方向极性、STEP/DIR 接线和 PA14/PA15 实际接线；
- 真实机构的有效行程、人工确认中心对应的机械位置和软限位是否安全；
- STEP 脉宽、速度、驱动器最小建立/保持时间和输入电平；
- 电机是否失步、每毫米脉冲数、细分档位和播放精度；
- 无 homing、无 encoder、无 Z lift，也没有失步检测或实体限位输入；
- `Steps`/路径坐标代表发出的脉冲和软件相对坐标，不是闭环绝对位置；
- LCD 当前由 UI 任务独占访问；如果增加其他画屏任务，必须增加互斥；
- 以上构建和主机测试不等于硬件验证，当前报告不宣称已在板上运行。

另外两条实现层面的约束，改动时留意：

- `PLOTTER_STEP_RATE_HZ` 必须 ≤ 1000 且最好是 1000 的因数，否则速率分频会静默失效（详见 §4.2 ②）；
- `User/plotter/plotter_path.c` 必须保持不依赖 STM32 头文件，否则 §7.2 的主机测试无法运行。

## 9. 排查与诊断

串口和屏幕在 bring-up 阶段都可能不可用。系统为此留了两块 **SRAM 黑匣子**——复位不掉，用调试器直接读，不需要任何外设先工作起来。

### 9.1 上电卡在哪一步：`g_boot_stage`

`User/boot_trace.h` 里的 `BOOT_STAGE(n)` 宏在启动路径上打点，值就是最后一个执行到的打点编号。读到的值比期望的小，说明卡在两者之间：

| 值 | 位置 |
| ---: | --- |
| 1 / 2 / 3 / 4 | `main` 进入 / `OSStart` 后 / 启动任务进入 / 两个应用任务已创建 |
| 10 / 11 / 12 | Motor 任务进入 / `Motor_Init` 完成 / `Key_Init` 完成 |
| 20–26 | UI 任务进入 / LCD 初始化 / GramScan / 触摸 I2C / 触摸面板 / 电机页绘制完成 |

最常用来区分的是 **11 与 20 之间**：卡在 11 之前说明电机或按键初始化出问题，卡在 12 与 20 之间说明 UI 任务没跑起来。

```bash
openocd -f openocd.cfg -c "init" -c "reset halt" \
        -c "mdw &g_boot_stage 1" -c "shutdown"
```

### 9.2 DIR 引脚有没有真的动：`g_dir_log`

「两个键都朝同一个方向转」有两种完全不同的成因，从现象上分不出来：

- 固件根本没让 DIR 引脚动 → 软件/引脚映射问题；
- 引脚动了，但驱动器没收到 → 线缆/驱动器问题。

`Motor_SetDir()` 每次调用记一条，按位打包（格式见 `User/motor/bsp_motor.h`）：

| 位 | 含义 |
| --- | --- |
| 0–7 | 序号低 8 位（用于排序） |
| 8–11 | 轴：0=X，1=Y |
| 12–15 | 命令方向：0=FORWARD，1=REVERSE |
| 16 | ODR —— 固件写下去的电平 |
| 17 | IDR —— 引脚实际读回来的电平 |

判读表：

| 现象 | 结论 |
| --- | --- |
| ODR 在 0/1 之间变，IDR 跟着变 | MCU 侧完全正常，问题在线缆或驱动器 |
| ODR 变了但 IDR 不变 | 引脚被外部拉住（短路/过载/损坏） |
| ODR 从来不变 | 软件或引脚映射错了 |

```bash
openocd -f openocd.cfg -c "init" -c "reset halt" \
        -c "mdw &g_dir_log_n 1" -c "mdw &g_dir_log 16" -c "shutdown"
```

### 9.3 其它常见现象

| 现象 | 先查 |
| --- | --- |
| 固件一跑就 SWD 失联 | 正常——PA14 占用 SWCLK，见 §5.2。用复位下连接 |
| 烧录“失败”但板子在跑 | 看 `verified N bytes`，别信退出码，见 §1 |
| 按键按住不动，必须松一下才行 | 上电武装机制，见 §3.2（防复位自走） |
| 播放结束后按键没反应 | 点动抑制锁：需两键都松开一次，见 §3.2 |
| 屏幕文字出现乱码/截断 | 屏上文字必须 ASCII，见 §6.1 |
| 示例菜单选中无反应 | `CENTER READY` 尚未确认，或路径已提交，见 §3.1 |
| 播放从 `START` 直接变 `REJECTED` | 点动仍在运行或有键被按住，见 §3.2；未开始运动时可退回 `PREVIEW` 重试 |
| 实际走速与配置值不符 | `PLOTTER_STEP_RATE_HZ` 不是 1000 的因数（如 300 实际跑 333 Hz），见 §4.2 ② |
| 改大 `PLOTTER_STEP_RATE_HZ` 却没有变快 | 超过 1000 会静默退化成 1 kHz，见 §4.2 ② |
