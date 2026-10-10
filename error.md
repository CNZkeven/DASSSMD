# 综合项目 Bug 修复记录

本文件记录 STM32F407 综合实验系统集成过程中已经修复的问题。工程当前目标平台为 STM32F407ZGT6，使用 uCOS-III、STM32F4xx 标准外设库、NT35510 LCD、GT9xx 电容触摸、USART1、ADC1、GPIO LED 与蜂鸣器。

## 当前状态

- 系统时钟按 25MHz HSE 配置，PLL 输出 168MHz SYSCLK。
- uCOS-III 启动后创建 `UI_Task` 和 `Worker_Task` 两个应用任务。
- LCD 通过 FSMC 驱动，主界面显示 5 个实验任务按钮。
- GT9xx 触摸中断只置位标志，I2C 读取放到 `UI_Task` 中执行。
- USART1 在任务创建前初始化，`printf` 通过 `_write()` 重定向到串口。
- ADC 使用任务轮询方式读取，不再使用高频 EOC 中断。
- 工程提供 `tests/test_static_contracts.py` 作为硬件难复现问题的静态回归检查。

## 已修复问题

### Fix #1 - ADC EOC 中断高频打断 uCOS-III

- 文件：`User/adc/bsp_adc.c`
- 问题：ADC 连续转换模式下开启 EOC 中断，转换完成中断频率很高，原 ISR 没有 `OSIntEnter()` / `OSIntExit()` 包裹，容易破坏 uCOS-III 的中断嵌套计数。
- 修复：取消 EOC 中断使用，改为在 `Worker_Task` 中轮询 `ADC_GetConversionValue()`；保留 `ADC_IRQHandler()` 作为兜底，并加入 OS 中断入口/出口。

### Fix #2 - ADC 任务读取旧值

- 文件：`User/task/worker_task.c`
- 问题：ADC 任务输出时没有主动刷新 ADC 转换寄存器，可能显示旧数据。
- 修复：`TASK_ADC` 和 `TASK_ADC_LED` 中先读取 `ADC_GetConversionValue(RHEOSTAT_ADC)`，再计算毫伏值。

### Fix #3 - ADC 使用浮点 printf 增加体积和依赖

- 文件：`User/task/worker_task.c`、`CMakeLists.txt`
- 问题：使用 `%.2f` 输出电压会拉入 nano newlib 的浮点格式化支持，增加固件体积，也让任务对浮点上下文更敏感。
- 修复：统一使用整数毫伏 `ADC_mV`，用 `%u.%03uV` 输出；CMake 不再链接 `_printf_float`。

### Fix #4 - ADC 数值只发串口，LCD 任务界面没有显示

- 文件：`User/task/worker_task.c`、`User/ui/ui_draw.c`、`User/ui/ui_draw.h`
- 问题：进入 ADC 任务界面后，用户只能从串口看到电压值，触摸屏上没有实时数据。
- 修复：新增 `UI_DrawAdcValues()`，在任务界面固定区域显示原始 ADC 值和电压值，并在刷新前清除旧区域。

### Fix #5 - ADC+LED 任务没有同步显示电压

- 文件：`User/task/worker_task.c`
- 问题：`TASK_ADC_LED` 只做 LED 阈值联动，不显示当前 ADC 值和电压。
- 修复：与 `TASK_ADC` 复用同一套整数毫伏计算、串口输出和 LCD 显示。

### Fix #6 - LED 上电默认全亮

- 文件：`User/led/bsp_led.c`
- 问题：板载 LED 为低电平点亮，初始化时拉低 PF6/PF7/PF8 会导致三灯常亮，也掩盖 fault 指示。
- 修复：初始化完成后使用 `GPIO_SetBits()` 拉高 PF6/PF7/PF8，默认关闭所有 LED。

### Fix #7 - LED 任务退出后状态残留

- 文件：`User/task/worker_task.c`
- 问题：从 GPIO LED 或 ADC+LED 任务返回主界面时，LED 可能保持上一个任务的状态。
- 修复：新增 `Worker_AllLedsOff()`，`Worker_CleanupTask()` 对 LED 相关任务统一关灯。

### Fix #8 - GPIO LED 任务只闪一个灯

- 文件：`User/task/worker_task.c`
- 问题：GPIO LED 实验原来只周期翻转单个 LED，不能体现三路 LED 的不同节拍。
- 修复：新增 `GPIO_LED1_TICKS`、`GPIO_LED2_TICKS`、`GPIO_LED3_TICKS`，分别按 1s、5s、10s 节拍翻转 LED1/LED2/LED3。

### Fix #9 - UART 任务周期性发送固定字符

- 文件：`User/task/worker_task.c`、`User/stm32f4xx_it.c`、`User/usart/bsp_usart.c`
- 问题：UART 任务原来每秒发送固定字符 `C`，不是真正的通信响应。
- 修复：USART1 开启 RXNE 中断；收到字节后在 `DEBUG_USART_IRQHandler()` 中按当前任务判断回显数据。

### Fix #10 - UART 任务缺少通信状态指示

- 文件：`User/task/worker_task.c`、`User/stm32f4xx_it.c`、`User/app_cfg.h`
- 问题：无法从板载 RGB LED 判断 UART 是否已经收到通信。
- 修复：新增 `g_uart_rx_received` 标志；进入 UART 任务时清零并点亮红灯，收到任意字节后中断置位，`Worker_Task` 切换为绿灯。

### Fix #11 - USART 发送等待可能永久阻塞

- 文件：`User/usart/bsp_usart.c`
- 问题：`Usart_SendByte()` 等待 TXE 标志时没有超时保护，串口异常时可能卡死任务。
- 修复：加入 `timeout` 计数，超时后跳出等待。

### Fix #12 - printf 没有真正输出到 USART1

- 文件：`User/usart/bsp_usart.c`
- 问题：工程使用 `nosys.specs`，默认 `_write()` 是空桩，`printf` 不会输出到串口。
- 修复：实现 `_write()`，将 stdout/stderr 每个字节发送到 USART1；补充弱实现 `_read/_close/_fstat/_isatty/_lseek`。

### Fix #13 - USART 初始化时机过晚

- 文件：`User/main.c`
- 问题：触摸初始化和任务启动早期可能调用 `printf`，但 USART 尚未可用。
- 修复：在 `AppTaskStart()` 中先调用 `Debug_USART_Config()`，再创建 UI/Worker 任务。

### Fix #14 - USART 中断没有 uCOS-III ISR 包裹

- 文件：`User/stm32f4xx_it.c`
- 问题：用户级 USART ISR 没有成对调用 `OSIntEnter()` / `OSIntExit()`。
- 修复：`DEBUG_USART_IRQHandler()` 外层加入 uCOS-III 中断入口/出口。

### Fix #15 - 蜂鸣器上电默认响

- 文件：`User/beep/bsp_beep.c`
- 问题：蜂鸣器为高电平响，GPIO 初始化后未明确关闭，可能上电即响。
- 修复：`BEEP_GPIO_Config()` 末尾调用 `Buzzer_OFF()`，默认静音。

### Fix #16 - 触摸 GPIO 初始化阶段过早打开 EXTI

- 文件：`User/touch/bsp_i2c_touch.c`
- 问题：触摸芯片复位和面板初始化尚未完成时打开 EXTI，可能提前触发触摸中断。
- 修复：`I2C_Touch_Init()` 中只完成 GPIO/I2C bit-bang 引脚和复位，并调用 `I2C_GTP_IRQDisable()`；正式初始化成功后再开中断。

### Fix #17 - 触摸 ISR 中执行完整 I2C 读写

- 文件：`User/stm32f4xx_it.c`、`User/ui/ui_task.c`
- 问题：`GTP_IRQHandler()` 中直接调用触摸处理会执行 bit-bang I2C 和重试逻辑，ISR 持续时间过长，影响系统实时性。
- 修复：ISR 只置位 `g_gtp_irq_flag` 并清 EXTI；`UI_Task` 主循环检测标志后调用 `GTP_TouchProcess()`。

### Fix #18 - 触摸处理函数在中断路径里绘制 LCD

- 文件：`User/touch/gt9xx.c`
- 问题：触摸 down/up 处理里直接调用 LCD 绘制，可能与 UI 任务抢 FSMC 总线。
- 修复：触摸底层只更新坐标和状态变量，界面跳转由 `UI_Task` 统一处理。

### Fix #19 - I2C bit-bang 延时可能被编译器优化

- 文件：`User/touch/bsp_i2c_touch.c`
- 问题：空循环延时没有 volatile 约束，GCC 优化后 I2C 时序可能过快。
- 修复：延时循环变量使用 `volatile`，保证软件 I2C 有稳定时序。

### Fix #20 - I2C 多字节读 ACK/NACK 时序错误

- 文件：`User/touch/bsp_i2c_touch.c`
- 问题：读取最后一个字节后没有按 I2C 规则发送 NACK，可能让从机继续保持传输状态。
- 修复：`I2C_ReadBytes()` 按 `bytes_remaining` 判断，中间字节 ACK，最后一个字节读完后 NACK，再 STOP。

### Fix #21 - I2C_Transfer 错误码方向错误

- 文件：`User/touch/gt9xx.c`
- 问题：底层 I2C 失败返回值可能被当作正数成功计数继续向上传播。
- 修复：失败路径返回负值，调用层据此判断错误。

### Fix #22 - 触摸 ID 越界可能访问轨迹数组

- 文件：`User/touch/gt9xx.c`
- 问题：触摸点 ID 未做边界检查，异常数据可能索引越界。
- 修复：`GTP_Touch_Down()` 和 `GTP_Touch_Up()` 中加入 `id < 0 || id >= GTP_MAX_TOUCH` 检查。

### Fix #23 - GTP 初始化顺序不安全

- 文件：`User/touch/gt9xx.c`
- 问题：触摸信息读取失败时仍可能继续打开中断；IRQ 使能早于必要信息确认。
- 修复：`GTP_Init_Panel()` 先执行 `GTP_Get_Info()` 并检查返回值，失败直接返回，成功后再 `I2C_GTP_IRQEnable()`。

### Fix #24 - GTP_Send_Command 忽略 command 参数

- 文件：`User/touch/gt9xx.c`
- 问题：发送触摸芯片命令时没有把函数参数写入命令缓冲区。
- 修复：设置 `command_buf[2] = command`。

### Fix #25 - UI 运行任务时仍响应主菜单按钮

- 文件：`User/ui/ui_task.c`
- 问题：进入任务界面后，如果触摸坐标落在原主菜单按钮区域，可能误切换任务。
- 修复：`UI_HandleTouch()` 在 `g_current_task != TASK_NONE` 时只处理返回按钮；只有主界面才扫描任务按钮。

### Fix #26 - UI 文本越界和标题遮挡

- 文件：`User/ui/ui_draw.c`、`User/ui/ui_draw.h`
- 问题：标题和按钮文字没有宽度限制，长字符串可能超出 480x800 竖屏区域或压到返回按钮。
- 修复：新增 `UI_DrawStringBounded()`，定义屏幕宽高、标题边距、返回按钮尺寸；主标题和任务标题分别使用不同 X 坐标。

### Fix #27 - SysTick 使用固定字面值

- 文件：`User/main.c`
- 问题：`OS_CPU_SysTickInit()` 若使用固定 tick reload 值，会与实际 `SystemCoreClock` 脱节。
- 修复：启动时调用 `SystemCoreClockUpdate()`，使用 `SystemCoreClock / OS_CFG_TICK_RATE_HZ` 初始化 SysTick。

### Fix #28 - HSE/PLL 配置与板卡实际晶振不一致

- 文件：`CMakeLists.txt`、`Libraries/CMSIS/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c`
- 问题：系统时钟配置与 25MHz HSE 不匹配会导致 SYSCLK、串口波特率和 uCOS tick 都偏差。
- 修复：CMake 定义 `HSE_VALUE=25000000`；STM32F40/41 分支使用 `PLL_M=25`、`PLL_N=336`、`PLL_P=2`，得到 168MHz SYSCLK。

### Fix #29 - Fault Handler 静默死循环

- 文件：`User/stm32f4xx_it.c`
- 问题：HardFault/MemManage/BusFault/UsageFault 直接死循环，实机上看不到崩溃类型。
- 修复：fault handler 直接配置 GPIOF 并点亮 LED 指示：HardFault=LED1，MemManage=LED2，BusFault=LED3，UsageFault=LED1+LED3。

### Fix #30 - UI/触摸处理栈空间偏小

- 文件：`User/app_cfg.h`
- 问题：UI 任务承担 LCD 绘制、触摸初始化、I2C 读写和字符串处理，原栈空间偏小。
- 修复：`UI_TASK_STK_SIZE` 增至 2048，`WORKER_TASK_STK_SIZE` 增至 1024。

### Fix #31 - GCC 端口缺少汇编/头文件兼容补丁

- 文件：`CMakeLists.txt`、`uCOSIII/uC-CPU/ARM-Cortex-M4/GCC/`、`uCOSIII/uCOS-III/Ports/ARM-Cortex-M4/Generic/GCC/`
- 问题：原工程偏 Keil/RealView 目录结构，直接用 GCC/CMake 构建缺少对应端口文件或头文件兼容。
- 修复：CMake 纳入 GCC startup、uCOS CPU 汇编、lib_mem 汇编和对应 include 路径；RealView 头文件补齐 GCC 编译需要的兼容定义。

## 回归检查

以下命令用于检查这些容易复发的集成问题：

```bash
pytest tests/test_static_contracts.py -q
```

当前检查覆盖触摸初始化顺序、I2C ACK/NACK、触摸 ID 越界、GTP 初始化顺序、HSE/PLL、SysTick、USART 初始化和超时、蜂鸣器极性、UI 边界、ADC 整数输出、LED 清理、UART 回显和 UART 红/绿通信状态等关键契约。

## 调试备注

- 板载 LED 为低电平点亮，写 `GPIO_ResetBits()` 是点亮，写 `GPIO_SetBits()` 是关闭。
- 蜂鸣器为高电平响，`GPIO_SetBits(PG7)` 是响，`GPIO_ResetBits(PG7)` 是静音。
- 触摸中断里不要做 I2C 读写、LCD 绘制或长时间 printf。
- 串口输出依赖 USART1 初始化，初始化顺序必须早于可能打印日志的任务。
- 如果 OpenOCD/SWD 因程序跑飞断连，可以降低 adapter speed，并按住 RESET 后重新连接。
