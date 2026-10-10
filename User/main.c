#include "stm32f4xx.h"
#include "os.h"
#include "os_cpu.h"
#include "os_cfg_app.h"
#include "ui_task.h"
#include "motor_task.h"
#include "app_cfg.h"
#include "bsp_usart.h"
#include "boot_trace.h"

/*
*********************************************************************************************************
*                                    上电进度标记（bring-up 用）
*
*  串口和屏幕在 bring-up 阶段都可能不可用，但 SRAM 在系统复位下不丢。
*  卡住时用调试器读 g_boot_stage 就知道启动走到哪一步了，用法见 boot_trace.h。
*********************************************************************************************************
*/
volatile uint32_t g_boot_stage = 0u;

/*
*********************************************************************************************************
*                                            CPU Timestamp Functions
*********************************************************************************************************
*/
CPU_TS CPU_TS_TmrRd(void) {
    return (CPU_TS)SysTick->VAL;
}

void CPU_TS_TmrInit(void) {
}

void OS_Dbg_Init(void) {
}

/*
*********************************************************************************************************
*                                            任务控制块
*********************************************************************************************************
*/
static OS_TCB   AppTaskStartTCB;
static OS_TCB   UITaskTCB;
static OS_TCB   MotorTaskTCB;

/*
*********************************************************************************************************
*                                            任务栈
*********************************************************************************************************
*/
static CPU_STK  AppTaskStartStk[256];
static CPU_STK  UITaskStk[UI_TASK_STK_SIZE];
static CPU_STK  MotorTaskStk[MOTOR_TASK_STK_SIZE];

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

    BOOT_STAGE(BOOT_STAGE_MAIN_ENTERED);

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

    BOOT_STAGE(BOOT_STAGE_TASK_START_ENTERED);

    // 初始化CPU
    CPU_Init();

    // 统一设置NVIC优先级分组 - 整个系统只配置一次
    // uCOS-III 要求使用Group_4（全部4位用于抢占优先级）
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    // 初始化SysTick
    SystemCoreClockUpdate();
    OS_CPU_SysTickInit(SystemCoreClock / OS_CFG_TICK_RATE_HZ);

    // GTP_Init_Panel() 会通过 printf 输出诊断信息，任务创建前先让 USART 可用。
    Debug_USART_Config();

    // 创建应用任务
    AppTaskCreate();
    BOOT_STAGE(BOOT_STAGE_TASKS_CREATED);

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

    // 创建电机点动任务
    // 优先级比 UI 高：松开按键就是要停，不能排在画屏和触摸屏软件 I2C 后面。
    OSTaskCreate((OS_TCB     *)&MotorTaskTCB,
                 (CPU_CHAR   *)"Motor Task",
                 (OS_TASK_PTR ) Motor_Task,
                 (void       *) 0,
                 (OS_PRIO     ) MOTOR_TASK_PRIO,
                 (CPU_STK    *)&MotorTaskStk[0],
                 (CPU_STK_SIZE) MOTOR_TASK_STK_SIZE / 10,
                 (CPU_STK_SIZE) MOTOR_TASK_STK_SIZE,
                 (OS_MSG_QTY  ) 0u,
                 (OS_TICK     ) 0u,
                 (void       *) 0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&os_err);
}
