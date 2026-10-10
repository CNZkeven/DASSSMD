#ifndef __UI_TASK_H
#define __UI_TASK_H

#include "stm32f4xx.h"

typedef enum {
    UI_PAGE_MOTOR = 0,
    UI_DRAW_CENTER_CONFIRM,
    UI_DRAW_READY,
    UI_DRAW_DEMO_MENU,
    UI_DRAW_CAPTURING,
    UI_DRAW_PREVIEW,
    UI_DRAW_INVALID,
    UI_DRAW_PLAYING,
    UI_DRAW_RESULT
} UI_State_t;

/*
*********************************************************************************************************
*                                            UI 任务
*
 *  负责 LCD/触摸初始化、电机页和单笔作画状态机。
*********************************************************************************************************
*/
void UI_Task(void *p_arg);

#endif /* __UI_TASK_H */
