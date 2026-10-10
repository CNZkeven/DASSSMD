# STM32F407 综合实验系统实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将5个独立的STM32F407实验项目整合为一个基于uCOS-III RTOS的综合系统，通过触摸屏界面选择运行不同的任务。

**Architecture:** 双任务架构，UI_Task负责触摸屏界面和任务切换，Worker_Task负责执行当前选中的功能任务。使用全局变量g_current_task进行任务间通信。

**Tech Stack:** STM32F407, uCOS-III, NT35510 LCD, GT9xx触摸屏, CMake, arm-none-eabi-gcc

---

## 文件结构

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

---

## Task 1: 复制基础文件和uCOS-III内核

**Files:**
- Create: `综合/uCOSIII/` (整个目录)
- Create: `综合/Libraries/` (整个目录)

- [ ] **Step 1: 复制uCOS-III内核文件**

```bash
# 从参考项目复制uCOS-III内核
cp -r "/home/kevenchow/桌面/f407/野火【STM32F407开发板-霸天虎】资料/0-开机例程源码/ucos_emWIN_开机例程/0-开机例程（即综合测试例程）/uCOSIII" "/home/kevenchow/桌面/f407/四个项实验代码/综合/"
```

- [ ] **Step 2: 复制STM32标准库文件**

```bash
# 从参考项目复制标准库
cp -r "/home/kevenchow/桌面/f407/野火【STM32F407开发板-霸天虎】资料/0-开机例程源码/ucos_emWIN_开机例程/0-开机例程（即综合测试例程）/Libraries" "/home/kevenchow/桌面/f407/四个项实验代码/综合/"
```

- [ ] **Step 3: 验证文件复制**

```bash
ls -la "/home/kevenchow/桌面/f407/四个项实验代码/综合/uCOSIII"
ls -la "/home/kevenchow/桌面/f407/四个项实验代码/综合/Libraries"
```

- [ ] **Step 4: Commit**

```bash
cd "/home/kevenchow/桌面/f407/四个项实验代码/综合"
git add uCOSIII/ Libraries/
git commit -m "feat: add uCOS-III kernel and STM32 standard library"
```

---

## Task 2: 复制LCD和触摸屏驱动

**Files:**
- Create: `综合/User/lcd/bsp_nt35510_lcd.h`
- Create: `综合/User/lcd/bsp_nt35510_lcd.c`
- Create: `综合/User/touch/bsp_i2c_touch.h`
- Create: `综合/User/touch/bsp_i2c_touch.c`
- Create: `综合/User/touch/gt9xx.h`
- Create: `综合/User/touch/gt9xx.c`
- Create: `综合/User/touch/palette.h`
- Create: `综合/User/touch/palette.c`
- Create: `综合/User/font/fonts.h`
- Create: `综合/User/font/fonts.c`
- Create: `综合/User/flash/bsp_spi_flash.h`
- Create: `综合/User/flash/bsp_spi_flash.c`

- [ ] **Step 1: 创建目录结构**

```bash
mkdir -p "/home/kevenchow/桌面/f407/四个项实验代码/综合/User/lcd"
mkdir -p "/home/kevenchow/桌面/f407/四个项实验代码/综合/User/touch"
mkdir -p "/home/kevenchow/桌面/f407/四个项实验代码/综合/User/font"
mkdir -p "/home/kevenchow/桌面/f407/四个项实验代码/综合/User/flash"
```

- [ ] **Step 2: 复制LCD驱动文件**

```bash
cp "/home/kevenchow/桌面/f407/野火【STM32F407开发板-霸天虎】资料/5-配套模块资料/屏幕模块/野火【电容触摸屏_MCU_4.5_4.3寸】模块资料/电容触摸屏—触摸画板_4.3寸/User/lcd/"* "/home/kevenchow/桌面/f407/四个项实验代码/综合/User/lcd/"
```

- [ ] **Step 3: 复制触摸屏驱动文件**

```bash
cp "/home/kevenchow/桌面/f407/野火【STM32F407开发板-霸天虎】资料/5-配套模块资料/屏幕模块/野火【电容触摸屏_MCU_4.5_4.3寸】模块资料/电容触摸屏—触摸画板_4.3寸/User/touch/"* "/home/kevenchow/桌面/f407/四个项实验代码/综合/User/touch/"
```

- [ ] **Step 4: 复制字体和Flash驱动**

```bash
cp "/home/kevenchow/桌面/f407/野火【STM32F407开发板-霸天虎】资料/5-配套模块资料/屏幕模块/野火【电容触摸屏_MCU_4.5_4.3寸】模块资料/电容触摸屏—触摸画板_4.3寸/User/font/"* "/home/kevenchow/桌面/f407/四个项实验代码/综合/User/font/"
cp "/home/kevenchow/桌面/f407/野火【STM32F407开发板-霸天虎】资料/5-配套模块资料/屏幕模块/野火【电容触摸屏_MCU_4.5_4.3寸】模块资料/电容触摸屏—触摸画板_4.3寸/User/flash/"* "/home/kevenchow/桌面/f407/四个项实验代码/综合/User/flash/"
```

- [ ] **Step 5: 验证文件复制**

```bash
ls -la "/home/kevenchow/桌面/f407/四个项实验代码/综合/User/lcd"
ls -la "/home/kevenchow/桌面/f407/四个项实验代码/综合/User/touch"
ls -la "/home/kevenchow/桌面/f407/四个项实验代码/综合/User/font"
ls -la "/home/kevenchow/桌面/f407/四个项实验代码/综合/User/flash"
```

- [ ] **Step 6: Commit**

```bash
cd "/home/kevenchow/桌面/f407/四个项实验代码/综合"
git add User/lcd/ User/touch/ User/font/ User/flash/
git commit -m "feat: add LCD and touchscreen driver files"
```

---

## Task 3: 复制各任务的驱动文件

**Files:**
- Create: `综合/User/beep/bsp_beep.h`
- Create: `综合/User/beep/bsp_beep.c`
- Create: `综合/User/usart/bsp_usart.h`
- Create: `综合/User/usart/bsp_usart.c`
- Create: `综合/User/led/bsp_led.h`
- Create: `综合/User/led/bsp_led.c`
- Create: `综合/User/adc/bsp_adc.h`
- Create: `综合/User/adc/bsp_adc.c`

- [ ] **Step 1: 复制蜂鸣器驱动**

```bash
cp "/home/kevenchow/桌面/f407/四个项实验代码/综合/User/beep/"* "/home/kevenchow/桌面/f407/四个项实验代码/综合/User/beep/"
```

- [ ] **Step 2: 复制UART驱动**

```bash
cp "/home/kevenchow/桌面/f407/四个项实验代码/uart/User/usart/"* "/home/kevenchow/桌面/f407/四个项实验代码/综合/User/usart/"
```

- [ ] **Step 3: 复制LED驱动**

```bash
cp "/home/kevenchow/桌面/f407/四个项实验代码/gpio/User/LED/"* "/home/kevenchow/桌面/f407/四个项实验代码/综合/User/led/"
```

- [ ] **Step 4: 复制ADC驱动**

```bash
cp "/home/kevenchow/桌面/f407/四个项实验代码/adc/User/adc/"* "/home/kevenchow/桌面/f407/四个项实验代码/综合/User/adc/"
```

- [ ] **Step 5: 验证文件复制**

```bash
ls -la "/home/kevenchow/桌面/f407/四个项实验代码/综合/User/beep"
ls -la "/home/kevenchow/桌面/f407/四个项实验代码/综合/User/usart"
ls -la "/home/kevenchow/桌面/f407/四个项实验代码/综合/User/led"
ls -la "/home/kevenchow/桌面/f407/四个项实验代码/综合/User/adc"
```

- [ ] **Step 6: Commit**

```bash
cd "/home/kevenchow/桌面/f407/四个项实验代码/综合"
git add User/beep/ User/usart/ User/led/ User/adc/
git commit -m "feat: add peripheral driver files (beep, uart, led, adc)"
```

---

## Task 4: 创建应用配置文件

**Files:**
- Create: `综合/User/app_cfg.h`

- [ ] **Step 1: 创建app_cfg.h**

```c
#ifndef __APP_CFG_H
#define __APP_CFG_H

/*
*********************************************************************************************************
*                                            TASK PRIORITIES
*********************************************************************************************************
*/
#define UI_TASK_PRIO           4
#define WORKER_TASK_PRIO       6

/*
*********************************************************************************************************
*                                            TASK STACK SIZES
*********************************************************************************************************
*/
#define UI_TASK_STK_SIZE       512
#define WORKER_TASK_STK_SIZE   512

/*
*********************************************************************************************************
*                                            TASK IDs
*********************************************************************************************************
*/
typedef enum {
    TASK_NONE = 0,
    TASK_BUZZER,
    TASK_UART,
    TASK_ADC,
    TASK_GPIO_LED,
    TASK_ADC_LED,
    TASK_MAX
} TaskID_t;

/*
*********************************************************************************************************
*                                            GLOBAL VARIABLES
*********************************************************************************************************
*/
extern volatile TaskID_t g_current_task;

#endif /* __APP_CFG_H */
```

- [ ] **Step 2: Commit**

```bash
cd "/home/kevenchow/桌面/f407/四个项实验代码/综合"
git add User/app_cfg.h
git commit -m "feat: add application configuration header"
```

---

## Task 5: 创建UI任务头文件

**Files:**
- Create: `综合/User/ui/ui_task.h`

- [ ] **Step 1: 创建ui_task.h**

```c
#ifndef __UI_TASK_H
#define __UI_TASK_H

#include "stm32f4xx.h"
#include "app_cfg.h"

/*
*********************************************************************************************************
*                                            BUTTON DEFINITIONS
*********************************************************************************************************
*/
typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    TaskID_t task_id;
    const char *name;
    const char *desc;
} Button_t;

/*
*********************************************************************************************************
*                                            FUNCTION PROTOTYPES
*********************************************************************************************************
*/
void UI_Task(void *p_arg);

#endif /* __UI_TASK_H */
```

- [ ] **Step 2: Commit**

```bash
cd "/home/kevenchow/桌面/f407/四个项实验代码/综合"
git add User/ui/ui_task.h
git commit -m "feat: add UI task header file"
```

---

## Task 6: 创建UI绘图头文件

**Files:**
- Create: `综合/User/ui/ui_draw.h`

- [ ] **Step 1: 创建ui_draw.h**

```c
#ifndef __UI_DRAW_H
#define __UI_DRAW_H

#include "stm32f4xx.h"
#include "ui_task.h"

/*
*********************************************************************************************************
*                                            COLOR DEFINITIONS
*********************************************************************************************************
*/
#define COLOR_BG            0x1A1A2E  // 深蓝色背景
#define COLOR_BTN_BG        0x16213E  // 深灰色按钮背景
#define COLOR_BTN_BORDER    0x0F3460  // 青色按钮边框
#define COLOR_TEXT          0xFFFFFF  // 白色文字
#define COLOR_SELECTED      0x4CC9F0  // 亮蓝色选中状态
#define COLOR_TITLE_BG      0x0F3460  // 标题栏背景

/*
*********************************************************************************************************
*                                            FUNCTION PROTOTYPES
*********************************************************************************************************
*/
void UI_DrawMainMenu(void);
void UI_DrawTaskView(const Button_t *btn);
void UI_DrawButton(const Button_t *btn, uint8_t selected);
void UI_DrawTitle(const char *title);
void UI_DrawBackButton(void);
void UI_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bgcolor);
void UI_DrawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
void UI_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);

#endif /* __UI_DRAW_H */
```

- [ ] **Step 2: Commit**

```bash
cd "/home/kevenchow/桌面/f407/四个项实验代码/综合"
git add User/ui/ui_draw.h
git commit -m "feat: add UI drawing header file"
```

---

## Task 7: 创建UI绘图实现

**Files:**
- Create: `综合/User/ui/ui_draw.c`

- [ ] **Step 1: 创建ui_draw.c**

```c
#include "ui_draw.h"
#include "bsp_nt35510_lcd.h"
#include "fonts.h"

/*
*********************************************************************************************************
*                                            任务按钮定义
*********************************************************************************************************
*/
static const Button_t task_buttons[5] = {
    {40,  80,  400, 100, TASK_BUZZER,   "蜂鸣器控制",  "周期性开关蜂鸣器"},
    {40,  200, 400, 100, TASK_UART,     "UART串口通信", "串口数据收发测试"},
    {40,  320, 400, 100, TASK_ADC,      "ADC电压采集",  "电位器电压测量显示"},
    {40,  440, 400, 100, TASK_GPIO_LED, "GPIO LED控制", "多LED不同频率闪烁"},
    {40,  560, 400, 100, TASK_ADC_LED,  "ADC+LED联动",  "电压超阈值点亮LED"},
};

/*
*********************************************************************************************************
*                                            绘制矩形
*********************************************************************************************************
*/
void UI_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
    uint16_t i, j;
    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
            NT35510_WritePoint(x + j, y + i, color);
        }
    }
}

/*
*********************************************************************************************************
*                                            绘制矩形边框
*********************************************************************************************************
*/
void UI_DrawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
    uint16_t i;
    // 上边
    for (i = 0; i < width; i++) {
        NT35510_WritePoint(x + i, y, color);
    }
    // 下边
    for (i = 0; i < width; i++) {
        NT35510_WritePoint(x + i, y + height - 1, color);
    }
    // 左边
    for (i = 0; i < height; i++) {
        NT35510_WritePoint(x, y + i, color);
    }
    // 右边
    for (i = 0; i < height; i++) {
        NT35510_WritePoint(x + width - 1, y + i, color);
    }
}

/*
*********************************************************************************************************
*                                            绘制字符串
*********************************************************************************************************
*/
void UI_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bgcolor) {
    // 使用LCD驱动的字符串显示函数
    NT35510_DispString_EN(x, y, str);
}

/*
*********************************************************************************************************
*                                            绘制标题栏
*********************************************************************************************************
*/
void UI_DrawTitle(const char *title) {
    // 填充标题栏背景
    UI_FillRect(0, 0, 480, 60, COLOR_TITLE_BG);
    
    // 绘制标题文字
    UI_DrawString(120, 20, title, COLOR_TEXT, COLOR_TITLE_BG);
    
    // 绘制分隔线
    UI_FillRect(0, 60, 480, 2, COLOR_BTN_BORDER);
}

/*
*********************************************************************************************************
*                                            绘制返回按钮
*********************************************************************************************************
*/
void UI_DrawBackButton(void) {
    // 绘制返回按钮背景
    UI_FillRect(10, 10, 100, 40, COLOR_BTN_BG);
    UI_DrawRect(10, 10, 100, 40, COLOR_BTN_BORDER);
    
    // 绘制返回文字
    UI_DrawString(25, 22, "返回", COLOR_TEXT, COLOR_BTN_BG);
}

/*
*********************************************************************************************************
*                                            绘制任务按钮
*********************************************************************************************************
*/
void UI_DrawButton(const Button_t *btn, uint8_t selected) {
    uint16_t bg_color = selected ? COLOR_SELECTED : COLOR_BTN_BG;
    uint16_t border_color = selected ? COLOR_TEXT : COLOR_BTN_BORDER;
    
    // 绘制按钮背景
    UI_FillRect(btn->x, btn->y, btn->width, btn->height, bg_color);
    UI_DrawRect(btn->x, btn->y, btn->width, btn->height, border_color);
    
    // 绘制任务名称
    UI_DrawString(btn->x + 20, btn->y + 20, btn->name, COLOR_TEXT, bg_color);
    
    // 绘制任务描述
    UI_DrawString(btn->x + 20, btn->y + 60, btn->desc, COLOR_TEXT, bg_color);
}

/*
*********************************************************************************************************
*                                            绘制主界面
*********************************************************************************************************
*/
void UI_DrawMainMenu(void) {
    uint8_t i;
    
    // 清屏
    UI_FillRect(0, 0, 480, 800, COLOR_BG);
    
    // 绘制标题栏
    UI_DrawTitle("STM32F407 综合实验系统");
    
    // 绘制5个任务按钮
    for (i = 0; i < 5; i++) {
        UI_DrawButton(&task_buttons[i], 0);
    }
}

/*
*********************************************************************************************************
*                                            绘制任务运行界面
*********************************************************************************************************
*/
void UI_DrawTaskView(const Button_t *btn) {
    // 清屏
    UI_FillRect(0, 0, 480, 800, COLOR_BG);
    
    // 绘制标题栏
    UI_DrawTitle("当前任务");
    
    // 绘制返回按钮
    UI_DrawBackButton();
    
    // 绘制放大的任务图标区域
    UI_FillRect(140, 200, 200, 200, COLOR_BTN_BG);
    UI_DrawRect(140, 200, 200, 200, COLOR_BTN_BORDER);
    
    // 绘制任务名称
    UI_DrawString(180, 280, btn->name, COLOR_TEXT, COLOR_BTN_BG);
    
    // 绘制任务描述
    UI_DrawString(140, 450, btn->desc, COLOR_TEXT, COLOR_BG);
    
    // 绘制运行状态
    UI_DrawString(140, 500, "状态：运行中...", COLOR_SELECTED, COLOR_BG);
}
```

- [ ] **Step 2: Commit**

```bash
cd "/home/kevenchow/桌面/f407/四个项实验代码/综合"
git add User/ui/ui_draw.c
git commit -m "feat: add UI drawing implementation"
```

---

## Task 8: 创建UI任务实现

**Files:**
- Create: `综合/User/ui/ui_task.c`

- [ ] **Step 1: 创建ui_task.c**

```c
#include "ui_task.h"
#include "ui_draw.h"
#include "bsp_nt35510_lcd.h"
#include "gt9xx.h"
#include "app_cfg.h"
#include "os.h"

/*
*********************************************************************************************************
*                                            全局变量
*********************************************************************************************************
*/
volatile TaskID_t g_current_task = TASK_NONE;

/*
*********************************************************************************************************
*                                            触摸消抖变量
*********************************************************************************************************
*/
static uint32_t last_touch_time = 0;
#define TOUCH_DEBOUNCE_MS  200

/*
*********************************************************************************************************
*                                            外部按钮定义
*********************************************************************************************************
*/
extern const Button_t task_buttons[5];

/*
*********************************************************************************************************
*                                            触摸处理函数
*********************************************************************************************************
*/
static void UI_HandleTouch(void) {
    uint16_t x, y;
    OS_ERR os_err;
    
    if (GTP_GetTouchPoint(&x, &y)) {
        uint32_t now = OSTimeGet(&os_err);
        
        // 消抖检查
        if (now - last_touch_time < TOUCH_DEBOUNCE_MS) {
            return;
        }
        last_touch_time = now;
        
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

/*
*********************************************************************************************************
*                                            UI任务主函数
*********************************************************************************************************
*/
void UI_Task(void *p_arg) {
    OS_ERR os_err;
    
    (void)p_arg;
    
    // 初始化LCD
    NT35510_Init();
    NT35510_GramScan(6);
    
    // 初始化触摸屏
    GTP_Init_Panel();
    
    // 绘制主界面
    UI_DrawMainMenu();
    
    while (1) {
        // 扫描触摸屏
        UI_HandleTouch();
        
        // 延时让出CPU
        OSTimeDly(50, OS_OPT_TIME_DLY, &os_err);
    }
}
```

- [ ] **Step 2: Commit**

```bash
cd "/home/kevenchow/桌面/f407/四个项实验代码/综合"
git add User/ui/ui_task.c
git commit -m "feat: add UI task implementation"
```

---

## Task 9: 创建工作任务头文件

**Files:**
- Create: `综合/User/task/worker_task.h`

- [ ] **Step 1: 创建worker_task.h**

```c
#ifndef __WORKER_TASK_H
#define __WORKER_TASK_H

#include "stm32f4xx.h"
#include "app_cfg.h"

/*
*********************************************************************************************************
*                                            FUNCTION PROTOTYPES
*********************************************************************************************************
*/
void Worker_Task(void *p_arg);

#endif /* __WORKER_TASK_H */
```

- [ ] **Step 2: Commit**

```bash
cd "/home/kevenchow/桌面/f407/四个项实验代码/综合"
git add User/task/worker_task.h
git commit -m "feat: add worker task header file"
```

---

## Task 10: 创建工作任务实现

**Files:**
- Create: `综合/User/task/worker_task.c`

- [ ] **Step 1: 创建worker_task.c**

```c
#include "worker_task.h"
#include "bsp_beep.h"
#include "bsp_usart.h"
#include "bsp_led.h"
#include "bsp_adc.h"
#include "app_cfg.h"
#include "os.h"
#include <stdio.h>

/*
*********************************************************************************************************
*                                            全局变量
*********************************************************************************************************
*/
__IO uint16_t ADC_ConvertedValue;
float ADC_Vol;

/*
*********************************************************************************************************
*                                            上一次任务ID
*********************************************************************************************************
*/
static TaskID_t last_task = TASK_NONE;

/*
*********************************************************************************************************
*                                            任务清理函数
*********************************************************************************************************
*/
static void Worker_CleanupTask(TaskID_t task_id) {
    switch (task_id) {
        case TASK_BUZZER:
            Buzzer_OFF();
            break;
        case TASK_GPIO_LED:
            // LED保持当前状态
            break;
        default:
            break;
    }
}

/*
*********************************************************************************************************
*                                            工作任务主函数
*********************************************************************************************************
*/
void Worker_Task(void *p_arg) {
    OS_ERR os_err;
    
    (void)p_arg;
    
    // 初始化所有外设
    BEEP_GPIO_Config();
    Debug_USART_Config();
    LED_GPIO_Config();
    Rheostat_Init();
    
    while (1) {
        // 检查任务切换
        if (g_current_task != last_task) {
            Worker_CleanupTask(last_task);
            last_task = g_current_task;
        }
        
        // 执行当前任务
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
                OSTimeDly(100, OS_OPT_TIME_DLY, &os_err);
                break;
        }
    }
}
```

- [ ] **Step 2: Commit**

```bash
cd "/home/kevenchow/桌面/f407/四个项实验代码/综合"
git add User/task/worker_task.c
git commit -m "feat: add worker task implementation"
```

---

## Task 11: 创建主程序文件

**Files:**
- Modify: `综合/User/main.c`

- [ ] **Step 1: 创建main.c**

```c
#include "includes.h"
#include "ui_task.h"
#include "worker_task.h"
#include "app_cfg.h"

/*
*********************************************************************************************************
*                                            任务控制块
*********************************************************************************************************
*/
static OS_TCB   AppTaskStartTCB;
static OS_TCB   UITaskTCB;
static OS_TCB   WorkerTaskTCB;

/*
*********************************************************************************************************
*                                            任务栈
*********************************************************************************************************
*/
static CPU_STK  AppTaskStartStk[256];
static CPU_STK  UITaskStk[UI_TASK_STK_SIZE];
static CPU_STK  WorkerTaskStk[WORKER_TASK_STK_SIZE];

/*
*********************************************************************************************************
*                                            函数声明
*********************************************************************************************************
*/
static void AppTaskStart(void *p_arg);
static void AppTaskCreate(void);

/*
*********************************************************************************************************
*                                            主函数
*********************************************************************************************************
*/
int main(void) {
    OS_ERR os_err;
    
    // 初始化uCOS-III
    OSInit(&os_err);
    
    // 创建启动任务
    OSTaskCreate((OS_TCB     *)&AppTaskStartTCB,
                 (CPU_CHAR   *)"App Task Start",
                 (OS_TASK_PTR ) AppTaskStart,
                 (void       *) 0,
                 (OS_PRIO     ) 2,
                 (CPU_STK    *)&AppTaskStartStk[0],
                 (CPU_STK_SIZE) 256 / 10,
                 (CPU_STK_SIZE) 256,
                 (OS_MSG_QTY  ) 0u,
                 (OS_TICK     ) 0u,
                 (void       *) 0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&os_err);
    
    // 启动uCOS-III
    OSStart(&os_err);
    
    while (1);
}

/*
*********************************************************************************************************
*                                            启动任务
*********************************************************************************************************
*/
static void AppTaskStart(void *p_arg) {
    OS_ERR os_err;
    
    (void)p_arg;
    
    // 初始化CPU
    CPU_Init();
    
    // 创建应用任务
    AppTaskCreate();
    
    // 启动任务循环
    while (1) {
        OSTimeDly(1000, OS_OPT_TIME_DLY, &os_err);
    }
}

/*
*********************************************************************************************************
*                                            创建应用任务
*********************************************************************************************************
*/
static void AppTaskCreate(void) {
    OS_ERR os_err;
    
    // 创建UI任务
    OSTaskCreate((OS_TCB     *)&UITaskTCB,
                 (CPU_CHAR   *)"UI Task",
                 (OS_TASK_PTR ) UI_Task,
                 (void       *) 0,
                 (OS_PRIO     ) UI_TASK_PRIO,
                 (CPU_STK    *)&UITaskStk[0],
                 (CPU_STK_SIZE) UI_TASK_STK_SIZE / 10,
                 (CPU_STK_SIZE) UI_TASK_STK_SIZE,
                 (OS_MSG_QTY  ) 0u,
                 (OS_TICK     ) 0u,
                 (void       *) 0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&os_err);
    
    // 创建工作任务
    OSTaskCreate((OS_TCB     *)&WorkerTaskTCB,
                 (CPU_CHAR   *)"Worker Task",
                 (OS_TASK_PTR ) Worker_Task,
                 (void       *) 0,
                 (OS_PRIO     ) WORKER_TASK_PRIO,
                 (CPU_STK    *)&WorkerTaskStk[0],
                 (CPU_STK_SIZE) WORKER_TASK_STK_SIZE / 10,
                 (CPU_STK_SIZE) WORKER_TASK_STK_SIZE,
                 (OS_MSG_QTY  ) 0u,
                 (OS_TICK     ) 0u,
                 (void       *) 0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&os_err);
}
```

- [ ] **Step 2: Commit**

```bash
cd "/home/kevenchow/桌面/f407/四个项实验代码/综合"
git add User/main.c
git commit -m "feat: add main program with uCOS-III task creation"
```

---

## Task 12: 更新CMakeLists.txt

**Files:**
- Modify: `综合/CMakeLists.txt`

- [ ] **Step 1: 更新CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.20)

# ---------------------------------------------------------------------------
# Toolchain: arm-none-eabi-gcc (must be set BEFORE project())
# ---------------------------------------------------------------------------
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER   arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY      arm-none-eabi-objcopy CACHE FILEPATH "objcopy tool")
set(CMAKE_SIZE         arm-none-eabi-size    CACHE FILEPATH "size tool")

set(CMAKE_C_COMPILER_WORKS   TRUE)
set(CMAKE_ASM_COMPILER_WORKS TRUE)

# Project name (no Chinese characters)
project(zonghe C ASM)

# ---------------------------------------------------------------------------
# MCU flags  (Cortex-M4 with hardware FPU)
# ---------------------------------------------------------------------------
set(MCU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")

set(CMAKE_C_FLAGS   "${MCU_FLAGS} -Wall -fdata-sections -ffunction-sections")
set(CMAKE_ASM_FLAGS "${MCU_FLAGS} -x assembler-with-cpp")

set(CMAKE_C_FLAGS_DEBUG   "-Og -g -gdwarf-2")
set(CMAKE_C_FLAGS_RELEASE "-Os")

# ---------------------------------------------------------------------------
# Linker script
# ---------------------------------------------------------------------------
set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/STM32F407ZGTx_FLASH.ld")
set(CMAKE_EXE_LINKER_FLAGS
    "${MCU_FLAGS} -T${LINKER_SCRIPT} -Wl,--gc-sections -Wl,--print-memory-usage --specs=nano.specs --specs=nosys.specs"
)

# ---------------------------------------------------------------------------
# Compile definitions
# ---------------------------------------------------------------------------
add_compile_definitions(
    STM32F40_41xxx
    USE_STDPERIPH_DRIVER
    HSE_VALUE=8000000
)

# ---------------------------------------------------------------------------
# Include directories
# ---------------------------------------------------------------------------
set(CMSIS_DIR          "${CMAKE_SOURCE_DIR}/Libraries/CMSIS")
set(CMSIS_DEVICE_DIR   "${CMSIS_DIR}/Device/ST/STM32F4xx")
set(PERIPH_DIR         "${CMAKE_SOURCE_DIR}/Libraries/STM32F4xx_StdPeriph_Driver")
set(UCOS_DIR           "${CMAKE_SOURCE_DIR}/uCOSIII")

include_directories(
    ${CMAKE_SOURCE_DIR}/User
    ${CMAKE_SOURCE_DIR}/User/beep
    ${CMAKE_SOURCE_DIR}/User/usart
    ${CMAKE_SOURCE_DIR}/User/led
    ${CMAKE_SOURCE_DIR}/User/adc
    ${CMAKE_SOURCE_DIR}/User/lcd
    ${CMAKE_SOURCE_DIR}/User/touch
    ${CMAKE_SOURCE_DIR}/User/font
    ${CMAKE_SOURCE_DIR}/User/flash
    ${CMAKE_SOURCE_DIR}/User/ui
    ${CMAKE_SOURCE_DIR}/User/task
    ${CMSIS_DIR}/Include
    ${CMSIS_DEVICE_DIR}/Include
    ${PERIPH_DIR}/inc
    ${UCOS_DIR}/Source
    ${UCOS_DIR}/Ports/ARM-Cortex-M/ARMv7-M/GNU
    ${UCOS_DIR}/Cfg/Template
)

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------

# --- User application ---
set(USER_SOURCES
    ${CMAKE_SOURCE_DIR}/User/main.c
    ${CMAKE_SOURCE_DIR}/User/stm32f4xx_it.c
    ${CMAKE_SOURCE_DIR}/User/beep/bsp_beep.c
    ${CMAKE_SOURCE_DIR}/User/usart/bsp_usart.c
    ${CMAKE_SOURCE_DIR}/User/led/bsp_led.c
    ${CMAKE_SOURCE_DIR}/User/adc/bsp_adc.c
    ${CMAKE_SOURCE_DIR}/User/lcd/bsp_nt35510_lcd.c
    ${CMAKE_SOURCE_DIR}/User/touch/bsp_i2c_touch.c
    ${CMAKE_SOURCE_DIR}/User/touch/gt9xx.c
    ${CMAKE_SOURCE_DIR}/User/touch/palette.c
    ${CMAKE_SOURCE_DIR}/User/font/fonts.c
    ${CMAKE_SOURCE_DIR}/User/flash/bsp_spi_flash.c
    ${CMAKE_SOURCE_DIR}/User/ui/ui_task.c
    ${CMAKE_SOURCE_DIR}/User/ui/ui_draw.c
    ${CMAKE_SOURCE_DIR}/User/task/worker_task.c
)

# --- CMSIS: system init + startup (assembly) ---
set(CMSIS_SOURCES
    ${CMSIS_DEVICE_DIR}/Source/Templates/system_stm32f4xx.c
    ${CMSIS_DEVICE_DIR}/Source/Templates/gcc/startup_stm32f40xx.s
)

# --- StdPeriph Driver (only the modules actually used) ---
set(PERIPH_SOURCES
    ${PERIPH_DIR}/src/misc.c
    ${PERIPH_DIR}/src/stm32f4xx_gpio.c
    ${PERIPH_DIR}/src/stm32f4xx_rcc.c
    ${PERIPH_DIR}/src/stm32f4xx_syscfg.c
    ${PERIPH_DIR}/src/stm32f4xx_exti.c
    ${PERIPH_DIR}/src/stm32f4xx_usart.c
    ${PERIPH_DIR}/src/stm32f4xx_tim.c
    ${PERIPH_DIR}/src/stm32f4xx_pwr.c
    ${PERIPH_DIR}/src/stm32f4xx_flash.c
    ${PERIPH_DIR}/src/stm32f4xx_fsmc.c
    ${PERIPH_DIR}/src/stm32f4xx_i2c.c
    ${PERIPH_DIR}/src/stm32f4xx_spi.c
    ${PERIPH_DIR}/src/stm32f4xx_dma.c
    ${PERIPH_DIR}/src/stm32f4xx_adc.c
)

# --- uCOS-III ---
set(UCOS_SOURCES
    ${UCOS_DIR}/Source/os_core.c
    ${UCOS_DIR}/Source/os_flag.c
    ${UCOS_DIR}/Source/os_mem.c
    ${UCOS_DIR}/Source/os_msg.c
    ${UCOS_DIR}/Source/os_mutex.c
    ${UCOS_DIR}/Source/os_prio.c
    ${UCOS_DIR}/Source/os_q.c
    ${UCOS_DIR}/Source/os_sem.c
    ${UCOS_DIR}/Source/os_stat.c
    ${UCOS_DIR}/Source/os_task.c
    ${UCOS_DIR}/Source/os_tick.c
    ${UCOS_DIR}/Source/os_time.c
    ${UCOS_DIR}/Source/os_tmr.c
    ${UCOS_DIR}/Source/os_var.c
    ${UCOS_DIR}/Ports/ARM-Cortex-M/ARMv7-M/GNU/os_cpu_c.c
    ${UCOS_DIR}/Ports/ARM-Cortex-M/ARMv7-M/GNU/os_cpu_a.s
    ${UCOS_DIR}/Cfg/Template/os_app_hooks.c
    ${UCOS_DIR}/Cfg/Template/os_cfg.c
    ${UCOS_DIR}/Cfg/Template/os_cpu_c.c
    ${UCOS_DIR}/Cfg/Template/os_dbg.c
)

# ---------------------------------------------------------------------------
# Executable target
# ---------------------------------------------------------------------------
add_executable(${PROJECT_NAME}.elf
    ${USER_SOURCES}
    ${CMSIS_SOURCES}
    ${PERIPH_SOURCES}
    ${UCOS_SOURCES}
)

# ---------------------------------------------------------------------------
# Post-build: generate .hex and .bin
# ---------------------------------------------------------------------------
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Generating ${PROJECT_NAME}.hex and ${PROJECT_NAME}.bin"
)
```

- [ ] **Step 2: Commit**

```bash
cd "/home/kevenchow/桌面/f407/四个项实验代码/综合"
git add CMakeLists.txt
git commit -m "feat: update CMakeLists.txt with uCOS-III and LCD support"
```

---

## Task 13: 创建uCOSS配置文件

**Files:**
- Create: `综合/uCOSIII/Cfg/Template/os_cfg.c`
- Create: `综合/uCOSIII/Cfg/Template/os_cfg.h`
- Create: `综合/uCOSIII/Cfg/Template/os_app_hooks.c`

- [ ] **Step 1: 创建os_cfg.c**

```c
#include "os_cfg.h"
```

- [ ] **Step 2: 创建os_cfg.h**

```c
#ifndef OS_CFG_H
#define OS_CFG_H

/* ---------------------------- MISCELLANEOUS -------------------------- */
#define OS_CFG_APP_HOOKS_EN           1u
#define OS_CFG_ARG_CHK_EN             1u
#define OS_CFG_CALLED_FROM_ISR_CHK_EN 1u
#define OS_CFG_DBG_EN                 1u
#define OS_CFG_DYN_TICK_EN            0u
#define OS_CFG_INVALID_OS_CALLS_CHK_EN 1u
#define OS_CFG_ISR_POST_DEFERRED_EN   0u
#define OS_CFG_OBJ_TYPE_CHK_EN        1u
#define OS_CFG_STK_SIZE_MIN           64u
#define OS_CFG_TICK_EN                1u
#define OS_CFG_TICK_RATE              1000u
#define OS_CFG_TIME_DLY_HMSM_EN       1u
#define OS_CFG_TIME_DLY_MS_EN         1u
#define OS_CFG_TIME_DLY_S_EN          1u
#define OS_CFG_TIME_GET_SET_EN        1u
#define OS_CFG_TLS_TBL_SIZE           1u

/* ----------------------------- EVENT FLAGS --------------------------- */
#define OS_CFG_FLAG_EN                1u
#define OS_CFG_FLAG_DEL_EN            1u
#define OS_CFG_FLAG_MODE_CLR_EN       1u
#define OS_CFG_FLAG_PEND_ABORT_EN     1u

/* ------------------------------ MEMORY ------------------------------- */
#define OS_CFG_MEM_EN                 1u

/* ----------------------------- MONITORS ------------------------------ */
#define OS_CFG_MON_EN                 0u
#define OS_CFG_MON_MAX_MONITORS       1u

/* ------------------------------ MUTEXES ------------------------------ */
#define OS_CFG_MUTEX_EN               1u
#define OS_CFG_MUTEX_DEL_EN           1u
#define OS_CFG_MUTEX_PEND_ABORT_EN    1u

/* -------------------------- MESSAGE QUEUES --------------------------- */
#define OS_CFG_Q_EN                   1u
#define OS_CFG_Q_DEL_EN               1u
#define OS_CFG_Q_FLUSH_EN             1u
#define OS_CFG_Q_PEND_ABORT_EN        1u

/* ---------------------------- SEMAPHORES ----------------------------- */
#define OS_CFG_SEM_EN                 1u
#define OS_CFG_SEM_DEL_EN             1u
#define OS_CFG_SEM_PEND_ABORT_EN      1u
#define OS_CFG_SEM_SET_EN             1u

/* -------------------------- TASK MANAGEMENT -------------------------- */
#define OS_CFG_TASK_CHANGE_PRIO_EN    1u
#define OS_CFG_TASK_DEL_EN            1u
#define OS_CFG_TASK_Q_EN              1u
#define OS_CFG_TASK_PROFILE_EN        1u
#define OS_CFG_TASK_REG_TBL_SIZE      1u
#define OS_CFG_TASK_SEM_PEND_ABORT_EN 1u
#define OS_CFG_TASK_SUSPEND_EN        1u

/* -------------------------- TIME MANAGEMENT -------------------------- */
#define OS_CFG_TIME_DLY_HMSM_EN       1u

/* ------------------------- TIMER MANAGEMENT -------------------------- */
#define OS_CFG_TMR_EN                 1u
#define OS_CFG_TMR_DEL_EN             1u

#endif /* OS_CFG_H */
```

- [ ] **Step 3: 创建os_app_hooks.c**

```c
#include "os.h"

void App_OS_SetAllHooks(void) {
}

void App_OS_ClrAllHooks(void) {
}

void App_OS_TaskCreateHook(OS_TCB *p_tcb) {
    (void)p_tcb;
}

void App_OS_TaskDelHook(OS_TCB *p_tcb) {
    (void)p_tcb;
}

void App_OS_TaskReturnHook(OS_TCB *p_tcb) {
    (void)p_tcb;
}

void App_OS_TaskSwHook(void) {
}

void App_OS_IdleTaskHook(void) {
}

void App_OS_StatTaskHook(void) {
}

void App_OS_TaskInitHook(OS_TCB *p_tcb) {
    (void)p_tcb;
}
```

- [ ] **Step 4: Commit**

```bash
cd "/home/kevenchow/桌面/f407/四个项实验代码/综合"
git add uCOSIII/Cfg/Template/
git commit -m "feat: add uCOS-III configuration files"
```

---

## Task 14: 创建includes.h头文件

**Files:**
- Create: `综合/User/includes.h`

- [ ] **Step 1: 创建includes.h**

```c
#ifndef __INCLUDES_H
#define __INCLUDES_H

#include "stm32f4xx.h"
#include "os.h"
#include "os_cfg.h"
#include "app_cfg.h"

#endif /* __INCLUDES_H */
```

- [ ] **Step 2: Commit**

```bash
cd "/home/kevenchow/桌面/f407/四个项实验代码/综合"
git add User/includes.h
git commit -m "feat: add includes.h header file"
```

---

## Task 15: 编译测试

**Files:**
- Modify: `综合/CMakeLists.txt` (如需要)

- [ ] **Step 1: 创建构建目录**

```bash
mkdir -p "/home/kevenchow/桌面/f407/四个项实验代码/综合/build"
```

- [ ] **Step 2: 配置CMake**

```bash
cd "/home/kevenchow/桌面/f407/四个项实验代码/综合/build"
cmake ..
```

- [ ] **Step 3: 编译项目**

```bash
cd "/home/kevenchow/桌面/f407/四个项实验代码/综合/build"
make -j4
```

- [ ] **Step 4: 修复编译错误（如有）**

根据编译错误信息修复代码问题。

- [ ] **Step 5: 验证生成文件**

```bash
ls -la "/home/kevenchow/桌面/f407/四个项实验代码/综合/build/zonghe.elf"
ls -la "/home/kevenchow/桌面/f407/四个项实验代码/综合/build/zonghe.hex"
ls -la "/home/kevenchow/桌面/f407/四个项实验代码/综合/build/zonghe.bin"
```

- [ ] **Step 6: Commit**

```bash
cd "/home/kevenchow/桌面/f407/四个项实验代码/综合"
git add -A
git commit -m "feat: complete build system configuration"
```

---

## 自我审查

### 1. 规范覆盖检查
- [x] 文件结构 - Task 1-3
- [x] UI界面设计 - Task 6-8
- [x] 任务架构 - Task 4, 9-11
- [x] 触摸处理 - Task 8
- [x] 任务切换机制 - Task 10

### 2. 占位符扫描
- [x] 无TBD、TODO或未完成部分
- [x] 所有代码块完整
- [x] 所有命令明确

### 3. 类型一致性检查
- [x] TaskID_t枚举定义一致
- [x] Button_t结构体定义一致
- [x] 函数签名一致

---

## 执行选项

**计划完成并保存到 `docs/superpowers/plans/2026-05-21-stm32f407-integration.md`。两种执行方式：**

**1. 子代理驱动（推荐）** - 我为每个任务分发一个新的子代理，任务之间进行审查，快速迭代

**2. 内联执行** - 在当前会话中使用executing-plans执行任务，批量执行并设置检查点

**选择哪种方式？**
