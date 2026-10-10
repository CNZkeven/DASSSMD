#include "motor_task.h"
#include "bsp_key.h"
#include "boot_trace.h"
#include "os.h"

/*
*********************************************************************************************************
*                                        点动状态机
*
*      IDLE ──按键按下──> DIR_SETUP ──等满 2ms──> RUNNING ──松开──> IDLE
*                             │                      │
*                             └───── 期间松开 ────────┘
*
*  设计取舍：
*    · DIR 先建立、再出脉冲，中间留 MOTOR_DIR_SETUP_TICKS。驱动器需要
*      DIR 稳定之后才认 STEP，方向建立时间不足会丢步甚至反向走一步。
*    · 两个键同时按住 = 停。不做"谁先按谁赢"，因为那取决于扫描顺序，
*      行为不可预期；停下来是唯一安全且确定的选择。
*    · 运行中反向：先停、重设 DIR、重新等建立时间，不在运动中途翻 DIR。
*********************************************************************************************************
*/

typedef enum {
    JOG_IDLE = 0,
    JOG_DIR_SETUP,
    JOG_RUNNING
} JogState_t;

/* 界面通过 Motor_SelectAxis() 写，Motor_Task 读 */
static volatile MotorAxis_t s_selected_axis = MOTOR_AXIS_X;

/* 以下只由 Motor_Task 访问 */
static JogState_t  s_state = JOG_IDLE;
static MotorAxis_t s_active_axis = MOTOR_AXIS_X;
static MotorDir_t  s_dir = MOTOR_DIR_FORWARD;
static uint32_t    s_dir_setup_start = 0u;

/* 供界面查询的镜像量 */
static volatile uint8_t   s_running = 0u;
static volatile MotorDir_t s_dir_mirror = MOTOR_DIR_FORWARD;

/* UI publishes a frozen, caller-owned path; Motor_Task is its only consumer. */
static const PlotterPath_t * volatile s_plot_path = (const PlotterPath_t *)0;
static volatile MotorPlotState_t s_plot_task_state = MOTOR_PLOT_IDLE;
static volatile uint8_t s_plot_stop_requested = 0u;

/* Motor_Task only: block a held key from becoming a post-play jog command. */
static uint8_t s_plot_jog_inhibit = 0u;

/*
*********************************************************************************************************
*                                            轴选择
*********************************************************************************************************
*/
void Motor_SelectAxis(MotorAxis_t axis) {
    if (axis < MOTOR_AXIS_COUNT) {
        s_selected_axis = axis;
    }
}

MotorAxis_t Motor_GetSelectedAxis(void) {
    return s_selected_axis;
}

uint8_t Motor_JogIsRunning(void) {
    return s_running;
}

MotorDir_t Motor_JogGetDir(void) {
    return s_dir_mirror;
}

static void Plotter_PublishTerminal(MotorPlotState_t terminal_state) {
    CPU_SR_ALLOC();

    CPU_CRITICAL_ENTER();
    /* A STOP published after Plotter_Update's snapshot but before an
       eligibility rejection still cancels this pending request. Resolve it
       atomically with terminal publication so the stop cannot be erased. */
    if (terminal_state == MOTOR_PLOT_REJECTED &&
        s_plot_task_state == MOTOR_PLOT_PENDING &&
        s_plot_stop_requested != 0u) {
        terminal_state = MOTOR_PLOT_ABORTED;
    }

    /* Publish terminal ownership before releasing the borrowed path pointer. */
    s_plot_task_state = terminal_state;
    s_plot_path = (const PlotterPath_t *)0;
    s_plot_stop_requested = 0u;
    CPU_CRITICAL_EXIT();
}

uint8_t Motor_PlotterRequestStart(const PlotterPath_t *path) {
    CPU_SR_ALLOC();
    uint8_t accepted = 0u;

    if (path == (const PlotterPath_t *)0 ||
        path->state != PLOTTER_PATH_READY ||
        path->count < 2u || path->count > PLOTTER_MAX_POINTS) {
        return 0u;
    }

    CPU_CRITICAL_ENTER();
    if (s_plot_task_state == MOTOR_PLOT_IDLE ||
        s_plot_task_state == MOTOR_PLOT_DONE ||
        s_plot_task_state == MOTOR_PLOT_ABORTED ||
        s_plot_task_state == MOTOR_PLOT_REJECTED) {
        s_plot_path = path;
        s_plot_stop_requested = 0u;
        s_plot_task_state = MOTOR_PLOT_PENDING;
        accepted = 1u;
    }
    CPU_CRITICAL_EXIT();

    return accepted;
}

void Motor_PlotterRequestStop(void) {
    CPU_SR_ALLOC();

    CPU_CRITICAL_ENTER();
    if (s_plot_task_state == MOTOR_PLOT_PENDING ||
        s_plot_task_state == MOTOR_PLOT_RUNNING) {
        s_plot_stop_requested = 1u;
    }
    CPU_CRITICAL_EXIT();
}

MotorPlotState_t Motor_PlotterGetState(void) {
    return s_plot_task_state;
}

uint16_t Motor_PlotterGetProgress(void) {
    MotorPlotState_t task_state = Motor_PlotterGetState();

    if (task_state == MOTOR_PLOT_DONE) {
        return 100u;
    }
    if (task_state == MOTOR_PLOT_RUNNING) {
        MotorPlotEngineState_t engine_state = Motor_PlotterEngineGetState();

        if (engine_state == MOTOR_PLOT_ENGINE_DONE) {
            return 100u;
        }
        if (engine_state == MOTOR_PLOT_ENGINE_RUNNING ||
            engine_state == MOTOR_PLOT_ENGINE_STOPPING) {
            return Motor_PlotterEngineGetProgress();
        }
    }
    return 0u;
}

/* Process a published UI command and mirror the timer-engine lifecycle. */
static uint8_t Plotter_Update(void) {
    CPU_SR_ALLOC();
    MotorPlotState_t task_state;
    const PlotterPath_t *path;
    uint8_t stop_requested;
    uint8_t plot_owned_cycle = 0u;

    CPU_CRITICAL_ENTER();
    task_state = s_plot_task_state;
    path = s_plot_path;
    stop_requested = s_plot_stop_requested;
    CPU_CRITICAL_EXIT();

    if (task_state == MOTOR_PLOT_PENDING) {
        uint8_t can_start;

        s_plot_jog_inhibit = 1u;
        plot_owned_cycle = 1u;
        if (stop_requested != 0u) {
            Plotter_PublishTerminal(MOTOR_PLOT_ABORTED);
        } else {
            can_start = (uint8_t)(s_state == JOG_IDLE && s_running == 0u &&
                                  Key_IsAnyHeld() == 0u &&
                                  path != (const PlotterPath_t *)0 &&
                                  path->state == PLOTTER_PATH_READY);
            if (can_start == 0u) {
                Plotter_PublishTerminal(MOTOR_PLOT_REJECTED);
            } else {
                uint8_t start_cancelled = 0u;

                /* Keep PENDING visible until the engine confirms ownership.
                   A concurrent STOP stays latched and is forwarded on the next
                   task cycle after RUNNING is published. */
                CPU_CRITICAL_ENTER();
                start_cancelled = (uint8_t)(s_plot_task_state != MOTOR_PLOT_PENDING ||
                                            s_plot_stop_requested != 0u);
                CPU_CRITICAL_EXIT();

                if (start_cancelled != 0u) {
                    Plotter_PublishTerminal(MOTOR_PLOT_ABORTED);
                } else if (Motor_PlotterEngineStart(path) == 0u) {
                    Plotter_PublishTerminal(MOTOR_PLOT_REJECTED);
                } else {
                    CPU_CRITICAL_ENTER();
                    if (s_plot_task_state == MOTOR_PLOT_PENDING) {
                        /* Do not clear a STOP that arrived during engine start. */
                        s_plot_task_state = MOTOR_PLOT_RUNNING;
                    }
                    CPU_CRITICAL_EXIT();
                }
            }
        }
    } else if (task_state == MOTOR_PLOT_RUNNING) {
        MotorPlotEngineState_t engine_state;

        plot_owned_cycle = 1u;
        if (stop_requested != 0u) {
            CPU_CRITICAL_ENTER();
            s_plot_stop_requested = 0u;
            CPU_CRITICAL_EXIT();
            Motor_PlotterEngineRequestStop();
        }

        engine_state = Motor_PlotterEngineGetState();
        switch (engine_state) {
            case MOTOR_PLOT_ENGINE_RUNNING:
            case MOTOR_PLOT_ENGINE_STOPPING:
                break;

            case MOTOR_PLOT_ENGINE_DONE:
                Plotter_PublishTerminal(MOTOR_PLOT_DONE);
                break;

            case MOTOR_PLOT_ENGINE_ABORTED:
                Plotter_PublishTerminal(MOTOR_PLOT_ABORTED);
                break;

            case MOTOR_PLOT_ENGINE_IDLE:
            default:
                /* A lost or invalid engine state must never resume jogging. */
                Plotter_PublishTerminal(MOTOR_PLOT_ABORTED);
                break;
        }
    }

    /* Keep inhibition throughout playback, then require a debounced release
       of both keys before allowing a later, fresh press to start a jog. */
    if (s_plot_task_state != MOTOR_PLOT_PENDING &&
        s_plot_task_state != MOTOR_PLOT_RUNNING &&
        s_plot_jog_inhibit != 0u && Key_IsAnyHeld() == 0u) {
        s_plot_jog_inhibit = 0u;
    }

    return plot_owned_cycle;
}

/*
*********************************************************************************************************
*                                         一个扫描周期的状态推进
*********************************************************************************************************
*/
static void Jog_Update(void) {
    OS_ERR     err;
    uint32_t   now = OSTimeGet(&err);
    uint8_t    fwd = Key_IsHeld(KEY_ID_FORWARD);
    uint8_t    rev = Key_IsHeld(KEY_ID_REVERSE);
    uint8_t    both = (uint8_t)((fwd != 0u) && (rev != 0u));
    uint8_t    requested_valid = (uint8_t)((both == 0u) && ((fwd != 0u) || (rev != 0u)));
    MotorDir_t requested_dir = (fwd != 0u) ? MOTOR_DIR_FORWARD : MOTOR_DIR_REVERSE;


    /* ---- 该停的情况：两个键都松开 / 两个键同时按住 ---- */
    if (requested_valid == 0u) {
        if (s_state != JOG_IDLE) {
            Motor_StopPulse(s_active_axis);
        }
        s_state   = JOG_IDLE;
        s_running = 0u;
        return;
    }

    /* ---- 运行中被切轴：先停下来，不带着负载换轴 ----
       （界面在运行中本来就会拒绝切轴，这里是兜底。） */
    if ((s_state != JOG_IDLE) && (s_selected_axis != s_active_axis)) {
        Motor_StopPulse(s_active_axis);
        s_state   = JOG_IDLE;
        s_running = 0u;
        return;
    }

    switch (s_state) {
        case JOG_IDLE:
            /* 起一次新的点动：先把方向摆好，等建立时间，再出脉冲 */
            s_active_axis     = s_selected_axis;
            s_dir             = requested_dir;
            s_dir_mirror      = requested_dir;
            Motor_ResetSteps(s_active_axis);
            Motor_SetDir(s_active_axis, s_dir);
            s_dir_setup_start = now;
            s_state           = JOG_DIR_SETUP;
            break;

        case JOG_DIR_SETUP:
            if (requested_dir != s_dir) {
                /* 建立期间被换了方向：重新计建立时间 */
                s_dir             = requested_dir;
                s_dir_mirror      = requested_dir;
                Motor_SetDir(s_active_axis, s_dir);
                s_dir_setup_start = now;
            } else if ((uint32_t)(now - s_dir_setup_start) >= MOTOR_DIR_SETUP_TICKS) {
                Motor_StartPulse(s_active_axis);
                s_running = 1u;
                s_state   = JOG_RUNNING;
            } else {
                /* 还在等 DIR 建立 */
            }
            break;

        case JOG_RUNNING:
            if (requested_dir != s_dir) {
                /* 运动中途反向：停脉冲 → 重设 DIR → 重新等建立时间 */
                Motor_StopPulse(s_active_axis);
                s_dir             = requested_dir;
                s_dir_mirror      = requested_dir;
                Motor_SetDir(s_active_axis, s_dir);
                s_dir_setup_start = now;
                s_running         = 0u;
                s_state           = JOG_DIR_SETUP;
            }
            break;

        default:
            s_state   = JOG_IDLE;
            s_running = 0u;
            break;
    }
}

/*
*********************************************************************************************************
*                                        电机任务主函数
*********************************************************************************************************
*/
void Motor_Task(void *p_arg) {
    OS_ERR os_err;
    (void)p_arg;

    BOOT_STAGE(BOOT_STAGE_MOTOR_TASK_ENTERED);

    Motor_Init();
    BOOT_STAGE(BOOT_STAGE_MOTOR_INIT_DONE);

    Key_Init();
    BOOT_STAGE(BOOT_STAGE_KEY_INIT_DONE);

    while (1) {
        Key_Scan();     /* 10ms 采样一次，30ms 消抖 */
        if (Plotter_Update() == 0u && s_plot_jog_inhibit == 0u) {
            Jog_Update();
        }
        OSTimeDly(MOTOR_TASK_PERIOD_TICKS, OS_OPT_TIME_DLY, &os_err);
    }
}
