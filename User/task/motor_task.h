#ifndef __MOTOR_TASK_H
#define __MOTOR_TASK_H

#include "stm32f4xx.h"
#include "bsp_motor.h"

/*
*********************************************************************************************************
*                                        电机点动任务
*
*  这里是整个交互的核心：把"板载按键被按住"翻译成"某个轴朝某个方向持续出脉冲"。
*
*      KEY1 按住 → 当前选中的轴正转
*      KEY2 按住 → 当前选中的轴反转
*      松开     → 停
*
*  轴由触摸屏选择（Motor_SelectAxis）。运行中切轴会被拒绝：
*  状态机会先停下来，不会带着负载换轴。
*********************************************************************************************************
*/

#define MOTOR_TASK_PERIOD_TICKS  10u   /* uCOS tick = 1ms */

void        Motor_Task(void *p_arg);
void        Motor_SelectAxis(MotorAxis_t axis);
MotorAxis_t Motor_GetSelectedAxis(void);
uint8_t     Motor_JogIsRunning(void);
MotorDir_t  Motor_JogGetDir(void);

typedef enum {
    MOTOR_PLOT_IDLE = 0,
    MOTOR_PLOT_PENDING,
    MOTOR_PLOT_RUNNING,
    MOTOR_PLOT_DONE,
    MOTOR_PLOT_ABORTED,
    MOTOR_PLOT_REJECTED
} MotorPlotState_t;

uint8_t         Motor_PlotterRequestStart(const PlotterPath_t *path);
void            Motor_PlotterRequestStop(void);
MotorPlotState_t Motor_PlotterGetState(void);
uint16_t        Motor_PlotterGetProgress(void);

#endif /* __MOTOR_TASK_H */
