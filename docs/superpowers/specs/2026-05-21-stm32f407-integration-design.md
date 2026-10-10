# STM32F407 综合实验系统设计文档

## 1. 项目概述

### 1.1 目标
将5个独立的STM32F407实验项目（蜂鸣器、UART、ADC、GPIO LED、ADC+LED）整合为一个基于uCOS-III RTOS的综合系统，通过触摸屏界面选择运行不同的任务。

### 1.2 技术栈
- **MCU**: STM32F407ZGT6
- **RTOS**: uCOS-III
- **LCD**: NT35510 (4.3寸, 480x800)
- **触摸屏**: GT9xx电容触摸控制器
- **构建系统**: CMake + arm-none-eabi-gcc
- **外设库**: STM32F4xx标准外设库

## 2. 系统架构

### 2.1 双任务架构

```
┌─────────────────────────────────────────┐
│           uCOS-III 内核                  │
├─────────────────────────────────────────┤
│  Task 1: UI_Task (优先级4)              │
│  - 触摸屏扫描                            │
│  - 界面绘制                              │
│  - 任务切换控制                          │
├─────────────────────────────────────────┤
│  Task 2: Worker_Task (优先级6)          │
│  - 执行当前选中的功能                    │
│  - 蜂鸣器/UART/ADC/GPIO/ADC+LED        │
└─────────────────────────────────────────┘
```

### 2.2 任务间通信
- 使用全局变量 `g_current_task` 传递当前任务ID
- UI任务修改任务ID，工作任务检测并执行
- 无需复杂的消息队列或信号量

## 3. 文件结构

```
综合/
├── CMakeLists.txt              # 主构建文件
├── STM32F407ZGTx_FLASH.ld     # 链接脚本
├── User/
│   ├── main.c                  # 主程序入口
│   ├── stm32f4xx_it.c          # 中断处理
│   ├── app_cfg.h               # 应用配置
│   ├── ui/
│   │   ├── ui_task.h           # UI任务头文件
│   │   ├── ui_task.c           # UI任务实现
│   │   ├── ui_draw.h           # 绘图函数
│   │   └── ui_draw.c           # LCD绘制
│   ├── task/
│   │   ├── worker_task.h       # 工作任务头文件
│   │   └── worker_task.c       # 工作任务实现
│   ├── beep/
│   │   ├── bsp_beep.h
│   │   └── bsp_beep.c
│   ├── usart/
│   │   ├── bsp_usart.h
│   │   └── bsp_usart.c
│   ├── led/
│   │   ├── bsp_led.h
│   │   └── bsp_led.c
│   ├── adc/
│   │   ├── bsp_adc.h
│   │   └── bsp_adc.c
│   ├── lcd/
│   │   ├── bsp_nt35510_lcd.h
│   │   └── bsp_nt35510_lcd.c
│   └── touch/
│       ├── bsp_i2c_touch.h
│       ├── bsp_i2c_touch.c
│       ├── gt9xx.h
│       ├── gt9xx.c
│       ├── palette.h
│       └── palette.c
├── Libraries/                  # STM32标准库
└── uCOSIII/                    # uCOS-III内核
```

## 4. UI界面设计

### 4.1 主界面布局

```
┌──────────────────────────────────────┐
│         STM32F407 综合实验系统        │  ← 标题栏
├──────────────────────────────────────┤
│                                      │
│   ┌─────────────────────────────┐   │
│   │  🔔 蜂鸣器控制              │   │  ← 图标1
│   │     周期性开关蜂鸣器        │   │
│   └─────────────────────────────┘   │
│                                      │
│   ┌─────────────────────────────┐   │
│   │  📡 UART串口通信            │   │  ← 图标2
│   │     串口数据收发测试        │   │
│   └─────────────────────────────┘   │
│                                      │
│   ┌─────────────────────────────┐   │
│   │  📊 ADC电压采集             │   │  ← 图标3
│   │     电位器电压测量显示      │   │
│   └─────────────────────────────┘   │
│                                      │
│   ┌─────────────────────────────┐   │
│   │  💡 GPIO LED控制            │   │  ← 图标4
│   │     多LED不同频率闪烁       │   │
│   └─────────────────────────────┘   │
│                                      │
│   ┌─────────────────────────────┐   │
│   │  ⚡ ADC+LED联动             │   │  ← 图标5
│   │     电压超阈值点亮LED       │   │
│   └─────────────────────────────┘   │
│                                      │
└──────────────────────────────────────┘
```

### 4.2 任务运行界面

```
┌──────────────────────────────────────┐
│  [返回]          当前任务：蜂鸣器控制 │  ← 返回按钮+状态
├──────────────────────────────────────┤
│                                      │
│          ┌─────────────┐             │
│          │    🔔      │             │  ← 放大的图标
│          │  蜂鸣器控制 │             │
│          └─────────────┘             │
│                                      │
│        状态：运行中...               │
│                                      │
└──────────────────────────────────────┘
```

### 4.3 颜色方案
- 背景色：深蓝色 (#1a1a2e)
- 按钮背景：深灰色 (#16213e)
- 按钮边框：青色 (#0f3460)
- 文字颜色：白色 (#ffffff)
- 选中状态：亮蓝色 (#4cc9f0)

## 5. 任务定义

### 5.1 任务配置 (app_cfg.h)

```c
#define UI_TASK_PRIO           4
#define WORKER_TASK_PRIO       6
#define UI_TASK_STK_SIZE       512
#define WORKER_TASK_STK_SIZE   512

typedef enum {
    TASK_NONE = 0,
    TASK_BUZZER,
    TASK_UART,
    TASK_ADC,
    TASK_GPIO_LED,
    TASK_ADC_LED,
    TASK_MAX
} TaskID_t;
```

### 5.2 UI任务实现

```c
void UI_Task(void *p_arg) {
    OS_ERR os_err;
    
    // 初始化LCD和触摸屏
    NT35510_Init();
    GTP_Init_Panel();
    
    // 绘制主界面
    UI_DrawMainMenu();
    
    while (1) {
        // 扫描触摸屏
        UI_HandleTouch();
        
        // 更新界面显示
        UI_UpdateDisplay();
        
        // 延时让出CPU
        OSTimeDly(50, OS_OPT_TIME_DLY, &os_err);
    }
}
```

### 5.3 工作任务实现

```c
void Worker_Task(void *p_arg) {
    OS_ERR os_err;
    
    // 初始化所有外设
    BEEP_GPIO_Config();
    Debug_USART_Config();
    LED_GPIO_Config();
    Rheostat_Init();
    
    while (1) {
        switch (g_current_task) {
            case TASK_BUZZER:
                Buzzer_ON();
                OSTimeDly(500, OS_OPT_TIME_DLY, &os_err);
                Buzzer_OFF();
                OSTimeDly(500, OS_OPT_TIME_DLY, &os_err);
                break;
                
            case TASK_UART:
                Usart_SendByte(USART1, 'C');
                OSTimeDly(1000, OS_OPT_TIME_DLY, &os_err);
                break;
                
            case TASK_ADC:
                ADC_Vol = (float)ADC_ConvertedValue / 4096 * 3.3f;
                printf("ADC: 0x%04X, Voltage: %.2fV\r\n", 
                       ADC_ConvertedValue, ADC_Vol);
                OSTimeDly(500, OS_OPT_TIME_DLY, &os_err);
                break;
                
            case TASK_GPIO_LED:
                GPIO_ToggleBits(LED_PORT, LED1_PIN);
                OSTimeDly(1000, OS_OPT_TIME_DLY, &os_err);
                break;
                
            case TASK_ADC_LED:
                ADC_Vol = (float)ADC_ConvertedValue / 4096 * 3.3f;
                if (ADC_Vol > 2.0f) {
                    GPIO_ResetBits(GPIOF, GPIO_Pin_6);
                } else {
                    GPIO_SetBits(GPIOF, GPIO_Pin_6);
                }
                OSTimeDly(200, OS_OPT_TIME_DLY, &os_err);
                break;
                
            case TASK_NONE:
            default:
                Buzzer_OFF();
                OSTimeDly(100, OS_OPT_TIME_DLY, &os_err);
                break;
        }
    }
}
```

## 6. 触摸处理

### 6.1 按钮区域定义

```c
typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    TaskID_t task_id;
    const char *name;
    const char *desc;
} Button_t;

static const Button_t task_buttons[5] = {
    {40,  80,  400, 100, TASK_BUZZER,   "蜂鸣器控制",  "周期性开关蜂鸣器"},
    {40,  200, 400, 100, TASK_UART,     "UART串口通信", "串口数据收发测试"},
    {40,  320, 400, 100, TASK_ADC,      "ADC电压采集",  "电位器电压测量显示"},
    {40,  440, 400, 100, TASK_GPIO_LED, "GPIO LED控制", "多LED不同频率闪烁"},
    {40,  560, 400, 100, TASK_ADC_LED,  "ADC+LED联动",  "电压超阈值点亮LED"},
};
```

### 6.2 触摸检测流程

```c
static void UI_HandleTouch(void) {
    uint16_t x, y;
    
    if (GTP_GetTouchPoint(&x, &y)) {
        // 检测返回按钮（任务运行界面）
        if (g_current_task != TASK_NONE) {
            if (x >= 10 && x <= 110 && y >= 10 && y <= 60) {
                g_current_task = TASK_NONE;
                UI_DrawMainMenu();
                return;
            }
        }
        
        // 检测任务按钮（主界面）
        for (int i = 0; i < 5; i++) {
            if (x >= task_buttons[i].x && 
                x <= task_buttons[i].x + task_buttons[i].width &&
                y >= task_buttons[i].y && 
                y <= task_buttons[i].y + task_buttons[i].height) {
                
                g_current_task = task_buttons[i].task_id;
                UI_DrawTaskView(&task_buttons[i]);
                return;
            }
        }
    }
}
```

### 6.3 触摸消抖

```c
static uint32_t last_touch_time = 0;
#define TOUCH_DEBOUNCE_MS  200

if (GTP_GetTouchPoint(&x, &y)) {
    uint32_t now = OSTimeGet(&os_err);
    if (now - last_touch_time >= TOUCH_DEBOUNCE_MS) {
        last_touch_time = now;
        // 处理触摸事件
    }
}
```

## 7. 任务切换机制

### 7.1 切换流程

```
用户点击按钮
    │
    ├── UI_Task检测到触摸
    ├── 更新 g_current_task = 新任务ID
    ├── 切换到任务运行界面
    │
    └── Worker_Task检测到g_current_task变化
        ├── 停止当前任务（清理资源）
        └── 开始执行新任务
```

### 7.2 安全切换保护

```c
static TaskID_t last_task = TASK_NONE;

if (g_current_task != last_task) {
    // 清理上一个任务的资源
    switch (last_task) {
        case TASK_BUZZER:
            Buzzer_OFF();
            break;
        case TASK_GPIO_LED:
            // LED保持当前状态
            break;
    }
    last_task = g_current_task;
}
```

## 8. 实现步骤

### 8.1 阶段1：环境搭建
1. 复制综合项目作为基础
2. 添加uCOS-III内核文件
3. 配置CMakeLists.txt
4. 验证基本编译

### 8.2 阶段2：LCD驱动移植
1. 复制LCD驱动文件
2. 配置FSMC接口
3. 测试LCD显示

### 8.3 阶段3：触摸屏驱动移植
1. 复制触摸屏驱动文件
2. 配置I2C接口
3. 测试触摸功能

### 8.4 阶段4：UI任务实现
1. 实现UI_Task
2. 实现界面绘制函数
3. 实现触摸处理

### 8.5 阶段5：工作任务实现
1. 实现Worker_Task
2. 移植5个任务功能
3. 实现任务切换

### 8.6 阶段6：集成测试
1. 测试所有任务功能
2. 测试任务切换
3. 优化用户体验

## 9. 风险与注意事项

### 9.1 潜在风险
- LCD驱动与现有代码的兼容性
- 触摸屏I2C地址配置
- uCOS-III内存占用
- 任务切换时的资源竞争

### 9.2 注意事项
- 确保所有外设初始化在正确时机
- 注意中断优先级配置
- 避免在中断中执行耗时操作
- 合理设置任务栈大小

## 10. 成功标准

- [ ] 5个任务功能正常运行
- [ ] 触摸屏界面响应流畅
- [ ] 任务切换无卡顿
- [ ] 系统稳定运行无崩溃
