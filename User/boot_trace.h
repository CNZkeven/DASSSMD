#ifndef __BOOT_TRACE_H
#define __BOOT_TRACE_H

#include "stm32f4xx.h"

/*
*********************************************************************************************************
*                                        上电进度标记（bring-up 用）
*
*  记录"启动流程最后走到了哪一步"。串口和屏幕在 bring-up 阶段都可能不可用，
*  但 SRAM 在系统复位下不会丢 —— 所以用法是：
*
*      1. 运行固件几秒
*      2. 复位下连接并 halt（不要让它跑）：
*           openocd -f openocd.cfg -c "init" -c "reset halt" \
*                   -c "mdw &g_boot_stage 1" -c "shutdown"
*      3. 读回来的值就是最后一个 BOOT_STAGE(n) 的 n
*
*  取值见下面 BOOT_STAGE_* 定义。读到的值比期望的小，说明卡在两者之间。
*********************************************************************************************************
*/

#define BOOT_STAGE_MAIN_ENTERED          1u
#define BOOT_STAGE_OS_STARTED            2u
#define BOOT_STAGE_TASK_START_ENTERED    3u
#define BOOT_STAGE_TASKS_CREATED         4u

#define BOOT_STAGE_MOTOR_TASK_ENTERED   10u
#define BOOT_STAGE_MOTOR_INIT_DONE      11u
#define BOOT_STAGE_KEY_INIT_DONE        12u

#define BOOT_STAGE_UI_TASK_ENTERED      20u
#define BOOT_STAGE_LCD_INIT_DONE        21u
#define BOOT_STAGE_GRAMSCAN_DONE        22u
#define BOOT_STAGE_TOUCH_I2C_DONE       23u
#define BOOT_STAGE_TOUCH_PANEL_DONE     24u
#define BOOT_STAGE_MOTOR_PAGE_DRAWN     26u

extern volatile uint32_t g_boot_stage;

#define BOOT_STAGE(n)   (g_boot_stage = (uint32_t)(n))

#endif /* __BOOT_TRACE_H */
