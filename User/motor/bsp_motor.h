#ifndef __BSP_MOTOR_H
#define __BSP_MOTOR_H

#include "stm32f4xx.h"
#include "../plotter/plotter_path.h"

/*
*********************************************************************************************************
*                                   双轴步进电机 STEP/DIR 驱动
*
*  用 TIM2 的两路硬件 PWM 产生 STEP 脉冲，GPIO 控制 DIR。
*  这里只负责"出脉冲 / 停脉冲 / 改方向"，不含任何按键或界面逻辑。
*
*  ★ 轴 ↔ 引脚映射、方向极性都集中在 bsp_motor.c 顶部的两张表里。
*    实机接线或方向跟预期不一致时，只改那两张表，不用碰其它代码。
*********************************************************************************************************
*/

/* 轴编号。这里指的是机构上的实际轴，不是原理图上的"电机1/电机2"。 */
typedef enum {
    MOTOR_AXIS_X = 0,
    MOTOR_AXIS_Y = 1,
    MOTOR_AXIS_COUNT
} MotorAxis_t;

/* 旋转方向 */
typedef enum {
    MOTOR_DIR_FORWARD = 0,
    MOTOR_DIR_REVERSE = 1,
    MOTOR_DIR_COUNT
} MotorDir_t;

/*
 *  TIM2 时基：TIM2 挂在 APB1 上，APB1 = HCLK/4 = 42MHz，
 *  定时器时钟因此是 84MHz。
 *      步进频率 = 84MHz / (PSC+1) / (ARR+1)
*              = 84MHz / 84 / 1000 = 1kHz   → 每秒 1000 步
 *      高电平 = CCR / 1MHz = 500us
 */
#define MOTOR_TIM_PRESCALER   83u
#define MOTOR_TIM_PERIOD      999u
#define MOTOR_PULSE_CCR       500u

/* Plotter events are scheduled from the existing 1kHz TIM2 update clock. */
#define PLOTTER_STEP_RATE_HZ  1000u

/* DIR 建立到第一个 STEP 之间的最短等待。uCOS tick = 1ms，所以就是 2ms。 */
#define MOTOR_DIR_SETUP_TICKS 2u

typedef enum {
    MOTOR_PLOT_ENGINE_IDLE = 0,
    MOTOR_PLOT_ENGINE_RUNNING,
    MOTOR_PLOT_ENGINE_STOPPING,
    MOTOR_PLOT_ENGINE_DONE,
    MOTOR_PLOT_ENGINE_ABORTED
} MotorPlotEngineState_t;

/*
*********************************************************************************************************
*                                            接口
*********************************************************************************************************
*/
void        Motor_Init(void);
void        Motor_SetDir(MotorAxis_t axis, MotorDir_t dir);
void        Motor_StartPulse(MotorAxis_t axis);
void        Motor_StopPulse(MotorAxis_t axis);
void        Motor_StopAll(void);
uint8_t     Motor_IsPulsing(MotorAxis_t axis);
uint32_t    Motor_GetSteps(MotorAxis_t axis);
void        Motor_ResetSteps(MotorAxis_t axis);
const char *Motor_AxisName(MotorAxis_t axis);

/* TIM2 更新中断里调用，只做步数统计。 */
void        Motor_UpdateISR(void);

/* Bounded ISR-driven plotter engine. The caller retains path until terminal state. */
uint8_t              Motor_PlotterEngineStart(const PlotterPath_t *path);
void                 Motor_PlotterEngineRequestStop(void);
MotorPlotEngineState_t Motor_PlotterEngineGetState(void);
uint16_t             Motor_PlotterEngineGetProgress(void);

/*
*********************************************************************************************************
*                          DIR 引脚记录（bring-up 诊断）
*
*  "两个键都朝同一个方向转" 有两种完全不同的成因：
*     (a) 固件根本没让 DIR 引脚动     —— 软件/映射问题
*     (b) 引脚动了，但驱动器没收到    —— 接线/驱动器问题
*  这两者从现象上分不出来，但读一次 g_dir_log 就能分开。
*
*  每次 Motor_SetDir() 记一条，按位打包：
*
*      bit  0-7   序号低 8 位（用于排序，回绕）
*      bit  8-11  轴   0=X, 1=Y
*      bit 12-15  命令方向 0=FORWARD, 1=REVERSE
*      bit 16     ODR —— 固件写下去的电平
*      bit 17     IDR —— 引脚实际读回来的电平
*
*  判读：
*      ODR 在 0/1 之间变、IDR 跟着变  → MCU 侧完全正常，问题在线/驱动器
*      ODR 变了但 IDR 不变            → 引脚被外部拉住（短路/过载/损坏）
*      ODR 从来不变                   → 软件或引脚映射错了
*
*  回读方法与 g_boot_stage 相同（见 boot_trace.h），SRAM 在系统复位下不丢：
*      openocd -f openocd.cfg -c "init" -c "reset halt" \
*              -c "mdw &g_dir_log_n 1" -c "mdw &g_dir_log 16" -c "shutdown"
*********************************************************************************************************
*/
#define DIR_LOG_SEQ_SHIFT    0u
#define DIR_LOG_AXIS_SHIFT   8u
#define DIR_LOG_DIR_SHIFT   12u
#define DIR_LOG_ODR_SHIFT   16u
#define DIR_LOG_IDR_SHIFT   17u

#define DIR_LOG_LEN         32u

extern volatile uint32_t g_dir_log[DIR_LOG_LEN];
extern volatile uint32_t g_dir_log_n;

#endif /* __BSP_MOTOR_H */
