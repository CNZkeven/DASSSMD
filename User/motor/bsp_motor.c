#include "bsp_motor.h"

/*
*********************************************************************************************************
*                                      ★ 轴 ↔ 引脚映射表 ★
*
*  2026-10-08 实测确认（不是推断）：
*
*      X 轴：STEP = PA1 (TIM2_CH2)，DIR = PA15
*      Y 轴：STEP = PA2 (TIM2_CH3)，DIR = PA14
*
*  ── DIR 这一对踩过坑，记在这里免得以后再查一遍 ──
*
*  最初 PA14/PA15 两根 DIR 线是接反的（PA14 接到了 X 驱动器）。现象是
*  "KEY1 和 KEY2 都往同一个方向转"：选 X 轴时固件翻转的是 PA15，
*  而 X 驱动器真正的 DIR 挂在 PA14 上，从头到尾没被碰过。
*
*  判别方法（不用动线、不用重烧）：
*      1. 选 X 轴按键，记下方向
*      2. 切到 Y 轴按一次键 —— 这一步会翻转 PA14
*      3. 切回 X 轴再按
*     X 如果翻成了反方向，就说明两根 DIR 线接反了。
*  （本项目的 g_dir_log 也能直接看出来：ODR/IDR 都正常翻转，
*    说明 MCU 侧没问题，那问题就只可能在线或驱动器这一侧。）
*
*  已按上述把 PA14/PA15 两根线在单片机端对调，现在接线与下表一致。
*
*  ★ 如果以后又去动这两根线，必须同步改下面这张表，否则症状会重现。
*********************************************************************************************************
*/
static const struct {
    GPIO_TypeDef *dir_port;
    uint16_t      dir_pin;
    uint16_t      tim_channel;   /* TIM_Channel_2 / TIM_Channel_3 */
} s_motor_hw[MOTOR_AXIS_COUNT] = {
    /* X 轴 */ { GPIOA, GPIO_Pin_15, TIM_Channel_2 },
    /* Y 轴 */ { GPIOA, GPIO_Pin_14, TIM_Channel_3 },
};

/*
*********************************************************************************************************
*                                          ★ 方向极性 ★
*
*  MOTOR_DIR_FORWARD 时 DIR 引脚输出的电平。
*  实机上"正转/反转"跟按键对不上时，把对应轴这一行 0u 改 1u 即可。
*********************************************************************************************************
*/
#define MOTOR_X_DIR_FORWARD_LEVEL   0u
#define MOTOR_Y_DIR_FORWARD_LEVEL   0u

static const uint8_t s_dir_forward_level[MOTOR_AXIS_COUNT] = {
    MOTOR_X_DIR_FORWARD_LEVEL,
    MOTOR_Y_DIR_FORWARD_LEVEL,
};

/*
*********************************************************************************************************
*                                            内部状态
*********************************************************************************************************
*/
static uint8_t           s_initialized = 0u;
static volatile uint8_t  s_pulsing[MOTOR_AXIS_COUNT];
static volatile uint32_t s_steps[MOTOR_AXIS_COUNT];

/* Plotter state is shared with TIM2_IRQHandler; the path stays caller-owned. */
static volatile MotorPlotEngineState_t s_plot_state = MOTOR_PLOT_ENGINE_IDLE;
static volatile MotorPlotEngineState_t s_plot_terminal_state = MOTOR_PLOT_ENGINE_DONE;
static volatile uint8_t s_plot_stop_requested = 0u;
static volatile uint8_t s_plot_active_mask = 0u;
static volatile uint8_t s_plot_preload_mask = 0u;
static volatile uint8_t s_plot_rate_phase = 0u;
static volatile uint8_t s_plot_dir_setup_ticks = 0u;
static volatile uint8_t s_plot_pending_valid = 0u;
static volatile uint8_t s_plot_pending_mask = 0u;
static volatile int8_t s_plot_pending_dir_x = 0;
static volatile int8_t s_plot_pending_dir_y = 0;
static volatile uint8_t s_plot_dir_x_known = 0u;
static volatile uint8_t s_plot_dir_y_known = 0u;
static volatile int8_t s_plot_dir_x = 0;
static volatile int8_t s_plot_dir_y = 0;
static volatile uint32_t s_plot_total_major_steps = 0u;
static volatile uint32_t s_plot_completed_major_steps = 0u;
static PlotterDda_t s_plot_dda;

/* DIR 引脚记录，判读方法见 bsp_motor.h */
volatile uint32_t g_dir_log[DIR_LOG_LEN];
volatile uint32_t g_dir_log_n = 0u;

static void Motor_LogDirPin(MotorAxis_t axis, MotorDir_t dir) {
    uint32_t idx = g_dir_log_n & (DIR_LOG_LEN - 1u);
    uint16_t pin = s_motor_hw[axis].dir_pin;
    uint32_t odr = ((s_motor_hw[axis].dir_port->ODR & pin) != 0u) ? 1u : 0u;
    uint32_t idr = ((s_motor_hw[axis].dir_port->IDR & pin) != 0u) ? 1u : 0u;

    g_dir_log[idx] = ((g_dir_log_n & 0xFFu) << DIR_LOG_SEQ_SHIFT)
                   | ((uint32_t)axis            << DIR_LOG_AXIS_SHIFT)
                   | ((uint32_t)dir             << DIR_LOG_DIR_SHIFT)
                   | (odr                       << DIR_LOG_ODR_SHIFT)
                   | (idr                       << DIR_LOG_IDR_SHIFT);
    g_dir_log_n++;
}


static uint8_t Motor_PlotterOwnsTimer(void) {
    return (uint8_t)(s_plot_state == MOTOR_PLOT_ENGINE_RUNNING ||
                     s_plot_state == MOTOR_PLOT_ENGINE_STOPPING);
}

static void Motor_SetDirInternal(MotorAxis_t axis, MotorDir_t dir) {
    uint8_t level;

    if (axis >= MOTOR_AXIS_COUNT || dir >= MOTOR_DIR_COUNT) {
        return;
    }

    /* FORWARD takes the configured level; REVERSE takes its inverse. */
    level = (dir == MOTOR_DIR_FORWARD) ? s_dir_forward_level[axis]
                                       : (uint8_t)(!s_dir_forward_level[axis]);

    if (level != 0u) {
        GPIO_SetBits(s_motor_hw[axis].dir_port, s_motor_hw[axis].dir_pin);
    } else {
        GPIO_ResetBits(s_motor_hw[axis].dir_port, s_motor_hw[axis].dir_pin);
    }

    Motor_LogDirPin(axis, dir);
}


static uint32_t Motor_PlotterMeasurePath(const PlotterPath_t *path) {
    uint32_t total = 0u;
    uint16_t i;

    for (i = 0u; i + 1u < path->count; ++i) {
        int32_t dx = (int32_t)path->points[i + 1u].x_steps -
                     (int32_t)path->points[i].x_steps;
        int32_t dy = (int32_t)path->points[i + 1u].y_steps -
                     (int32_t)path->points[i].y_steps;
        uint32_t abs_dx = (uint32_t)(dx < 0 ? -dx : dx);
        uint32_t abs_dy = (uint32_t)(dy < 0 ? -dy : dy);
        total += abs_dx > abs_dy ? abs_dx : abs_dy;
    }

    return total;
}

static void Motor_PlotterWriteCompareMask(uint8_t mask) {
    if ((mask & PLOTTER_AXIS_X) != 0u) {
        TIM_SetCompare2(TIM2, MOTOR_PULSE_CCR);
    } else {
        TIM_SetCompare2(TIM2, 0u);
    }

    if ((mask & PLOTTER_AXIS_Y) != 0u) {
        TIM_SetCompare3(TIM2, MOTOR_PULSE_CCR);
    } else {
        TIM_SetCompare3(TIM2, 0u);
    }
}

static void Motor_PlotterCountCompletedMask(uint8_t mask) {
    if ((mask & PLOTTER_AXIS_X) != 0u) {
        ++s_steps[MOTOR_AXIS_X];
    }
    if ((mask & PLOTTER_AXIS_Y) != 0u) {
        ++s_steps[MOTOR_AXIS_Y];
    }
    if (mask != 0u && s_plot_completed_major_steps < s_plot_total_major_steps) {
        ++s_plot_completed_major_steps;
    }
}

static void Motor_PlotterScheduleISR(void) {
    uint8_t axis_mask;
    int8_t dir_x;
    int8_t dir_y;
    uint8_t direction_changed = 0u;

    if (s_plot_state == MOTOR_PLOT_ENGINE_RUNNING && s_plot_stop_requested != 0u) {
        s_plot_terminal_state = MOTOR_PLOT_ENGINE_ABORTED;
        s_plot_state = MOTOR_PLOT_ENGINE_STOPPING;
        s_plot_pending_valid = 0u;
        s_plot_pending_mask = 0u;
        s_plot_dir_setup_ticks = 0u;
    } else if (s_plot_state == MOTOR_PLOT_ENGINE_STOPPING &&
               s_plot_stop_requested != 0u) {
        s_plot_terminal_state = MOTOR_PLOT_ENGINE_ABORTED;
    }

    if (s_plot_state == MOTOR_PLOT_ENGINE_STOPPING) {
        s_plot_preload_mask = 0u;
        Motor_PlotterWriteCompareMask(0u);
        if (s_plot_active_mask == 0u && s_plot_preload_mask == 0u) {
            s_plot_state = s_plot_terminal_state;
        }
        return;
    }

    if (s_plot_state != MOTOR_PLOT_ENGINE_RUNNING) {
        /* IDLE/terminal states belong to jog mode; never overwrite its CCRs. */
        return;
    }

    if (s_plot_pending_valid != 0u) {
        if (s_plot_dir_setup_ticks > 0u) {
            --s_plot_dir_setup_ticks;
        }
        if (s_plot_dir_setup_ticks > 0u) {
            s_plot_preload_mask = 0u;
            Motor_PlotterWriteCompareMask(0u);
            return;
        }
        s_plot_preload_mask = s_plot_pending_mask;
        s_plot_pending_mask = 0u;
        s_plot_pending_valid = 0u;
        Motor_PlotterWriteCompareMask(s_plot_preload_mask);
        return;
    }

    ++s_plot_rate_phase;
    if (s_plot_rate_phase < (1000u / PLOTTER_STEP_RATE_HZ)) {
        s_plot_preload_mask = 0u;
        Motor_PlotterWriteCompareMask(0u);
        return;
    }
    s_plot_rate_phase = 0u;

    axis_mask = 0u;
    dir_x = 0;
    dir_y = 0;
    if (PlotterDda_Next(&s_plot_dda, &axis_mask, &dir_x, &dir_y) == 0u) {
        s_plot_terminal_state = MOTOR_PLOT_ENGINE_DONE;
        s_plot_state = MOTOR_PLOT_ENGINE_STOPPING;
        s_plot_preload_mask = 0u;
        Motor_PlotterWriteCompareMask(0u);
        if (s_plot_active_mask == 0u) {
            s_plot_state = MOTOR_PLOT_ENGINE_DONE;
        }
        return;
    }

    if ((axis_mask & PLOTTER_AXIS_X) != 0u &&
        (s_plot_dir_x_known == 0u || s_plot_dir_x != dir_x)) {
        Motor_SetDirInternal(MOTOR_AXIS_X,
                             dir_x > 0 ? MOTOR_DIR_FORWARD : MOTOR_DIR_REVERSE);
        s_plot_dir_x = dir_x;
        s_plot_dir_x_known = 1u;
        direction_changed = 1u;
    }
    if ((axis_mask & PLOTTER_AXIS_Y) != 0u &&
        (s_plot_dir_y_known == 0u || s_plot_dir_y != dir_y)) {
        Motor_SetDirInternal(MOTOR_AXIS_Y,
                             dir_y > 0 ? MOTOR_DIR_FORWARD : MOTOR_DIR_REVERSE);
        s_plot_dir_y = dir_y;
        s_plot_dir_y_known = 1u;
        direction_changed = 1u;
    }

    if (direction_changed != 0u) {
        s_plot_pending_mask = axis_mask;
        s_plot_pending_dir_x = dir_x;
        s_plot_pending_dir_y = dir_y;
        s_plot_pending_valid = 1u;
        s_plot_dir_setup_ticks = MOTOR_DIR_SETUP_TICKS;
        s_plot_rate_phase = 0u;
        s_plot_preload_mask = 0u;
        Motor_PlotterWriteCompareMask(0u);
        return;
    }

    s_plot_preload_mask = axis_mask;
    Motor_PlotterWriteCompareMask(s_plot_preload_mask);
}

/*
*********************************************************************************************************
*                                            初始化
*********************************************************************************************************
*/
void Motor_Init(void) {
    GPIO_InitTypeDef        gpio;
    TIM_TimeBaseInitTypeDef tb;
    TIM_OCInitTypeDef       oc;
    NVIC_InitTypeDef        nvic;
    uint32_t i;

    for (i = 0u; i < (uint32_t)MOTOR_AXIS_COUNT; i++) {
        s_pulsing[i] = 0u;
        s_steps[i]   = 0u;
    }
    s_plot_state = MOTOR_PLOT_ENGINE_IDLE;
    s_plot_terminal_state = MOTOR_PLOT_ENGINE_DONE;
    s_plot_stop_requested = 0u;
    s_plot_active_mask = 0u;
    s_plot_preload_mask = 0u;
    s_plot_rate_phase = 0u;
    s_plot_dir_setup_ticks = 0u;
    s_plot_pending_valid = 0u;
    s_plot_pending_mask = 0u;
    s_plot_dir_x_known = 0u;
    s_plot_dir_y_known = 0u;
    s_plot_dir_x = 0;
    s_plot_dir_y = 0;
    s_plot_total_major_steps = 0u;
    s_plot_completed_major_steps = 0u;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* ---- STEP: PA1( TIM2_CH2 ) / PA2( TIM2_CH3 )，复用推挽 ---- */
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource1, GPIO_AF_TIM2);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_TIM2);

    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin   = GPIO_Pin_1 | GPIO_Pin_2;
    gpio.GPIO_Mode  = GPIO_Mode_AF;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd  = GPIO_PuPd_UP;
    gpio.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOA, &gpio);

    /* ---- DIR: PA14 / PA15 推挽输出，默认低电平 ---- */
    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin   = GPIO_Pin_14 | GPIO_Pin_15;
    gpio.GPIO_Mode  = GPIO_Mode_OUT;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd  = GPIO_PuPd_UP;
    gpio.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOA, &gpio);
    GPIO_ResetBits(GPIOA, GPIO_Pin_14 | GPIO_Pin_15);

    /* ---- TIM2 时基：84MHz / 84 / 1000 = 1kHz ---- */
    TIM_TimeBaseStructInit(&tb);
    tb.TIM_Prescaler     = MOTOR_TIM_PRESCALER;
    tb.TIM_Period        = MOTOR_TIM_PERIOD;
    tb.TIM_ClockDivision = TIM_CKD_DIV1;
    tb.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &tb);

    /* ---- CH2 / CH3：PWM1，启动时 CCR=0 → 引脚恒为低电平 ---- */
    TIM_OCStructInit(&oc);
    oc.TIM_OCMode      = TIM_OCMode_PWM1;
    oc.TIM_OutputState = TIM_OutputState_Enable;
    oc.TIM_OCPolarity  = TIM_OCPolarity_High;
    oc.TIM_Pulse       = 0u;
    TIM_OC2Init(TIM2, &oc);
    TIM_OC3Init(TIM2, &oc);

    /* CCR 预装载：写 CCR 在下一个更新事件才生效，保证最后一个脉冲完整输出 */
    TIM_OC2PreloadConfig(TIM2, TIM_OCPreload_Enable);
    TIM_OC3PreloadConfig(TIM2, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM2, ENABLE);

    /* ---- 更新中断：只用于步数统计 ---- */
    nvic.NVIC_IRQChannel                   = TIM2_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 2;   /* 与 USART1 同级 */
    nvic.NVIC_IRQChannelSubPriority        = 0;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);

    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    TIM_Cmd(TIM2, ENABLE);
    s_initialized = 1u;
}

uint8_t Motor_PlotterEngineStart(const PlotterPath_t *path) {
    uint32_t total_major_steps;
    uint32_t i;

    if (s_plot_state != MOTOR_PLOT_ENGINE_IDLE &&
        s_plot_state != MOTOR_PLOT_ENGINE_DONE &&
        s_plot_state != MOTOR_PLOT_ENGINE_ABORTED) {
        return 0u;
    }
    if (s_initialized == 0u || path == (const PlotterPath_t *)0 ||
        path->state != PLOTTER_PATH_READY || path->count < 2u ||
        path->count > PLOTTER_MAX_POINTS) {
        return 0u;
    }
    for (i = 0u; i < (uint32_t)MOTOR_AXIS_COUNT; ++i) {
        if (s_pulsing[i] != 0u) {
            return 0u;
        }
    }

    total_major_steps = Motor_PlotterMeasurePath(path);
    if (total_major_steps == 0u) {
        return 0u;
    }

    PlotterDda_Begin(&s_plot_dda, path);
    if (s_plot_dda.active == 0u) {
        return 0u;
    }

    s_plot_stop_requested = 0u;
    s_plot_terminal_state = MOTOR_PLOT_ENGINE_DONE;
    s_plot_active_mask = 0u;
    s_plot_preload_mask = 0u;
    s_plot_rate_phase = 0u;
    s_plot_dir_setup_ticks = 0u;
    s_plot_pending_valid = 0u;
    s_plot_pending_mask = 0u;
    s_plot_pending_dir_x = 0;
    s_plot_pending_dir_y = 0;
    s_plot_dir_x_known = 0u;
    s_plot_dir_y_known = 0u;
    s_plot_dir_x = 0;
    s_plot_dir_y = 0;
    s_plot_completed_major_steps = 0u;
    s_plot_total_major_steps = total_major_steps;

    /* Publish RUNNING only after all task/ISR-shared state is ready. */
    s_plot_state = MOTOR_PLOT_ENGINE_RUNNING;
    return 1u;
}

void Motor_PlotterEngineRequestStop(void) {
    if (s_plot_state == MOTOR_PLOT_ENGINE_RUNNING ||
        s_plot_state == MOTOR_PLOT_ENGINE_STOPPING) {
        s_plot_stop_requested = 1u;
    }
}

MotorPlotEngineState_t Motor_PlotterEngineGetState(void) {
    return s_plot_state;
}

uint16_t Motor_PlotterEngineGetProgress(void) {
    uint32_t total = s_plot_total_major_steps;
    uint32_t completed = s_plot_completed_major_steps;

    if (s_plot_state == MOTOR_PLOT_ENGINE_DONE) {
        return 100u;
    }
    if (total == 0u) {
        return 0u;
    }
    if (completed >= total) {
        return 100u;
    }
    return (uint16_t)(((uint64_t)completed * 100u) / total);
}

/*
*********************************************************************************************************
*                                            方向控制
*********************************************************************************************************
*/
void Motor_SetDir(MotorAxis_t axis, MotorDir_t dir) {
    if (Motor_PlotterOwnsTimer() != 0u) {
        return;
    }
    Motor_SetDirInternal(axis, dir);
}

/*
*********************************************************************************************************
*                                        开始 / 停止出脉冲
*********************************************************************************************************
*/
void Motor_StartPulse(MotorAxis_t axis) {
    if (Motor_PlotterOwnsTimer() != 0u) {
        return;
    }
    if (axis >= MOTOR_AXIS_COUNT) {
        return;
    }

    if (s_motor_hw[axis].tim_channel == TIM_Channel_2) {
        TIM_SetCompare2(TIM2, MOTOR_PULSE_CCR);
    } else {
        TIM_SetCompare3(TIM2, MOTOR_PULSE_CCR);
    }

    s_pulsing[axis] = 1u;
}

void Motor_StopPulse(MotorAxis_t axis) {
    if (Motor_PlotterOwnsTimer() != 0u) {
        return;
    }
    if (axis >= MOTOR_AXIS_COUNT) {
        return;
    }

    /* CCR=0 → PWM1 模式下输出恒低。因预装载，当前这个脉冲会完整走完。 */
    if (s_motor_hw[axis].tim_channel == TIM_Channel_2) {
        TIM_SetCompare2(TIM2, 0u);
    } else {
        TIM_SetCompare3(TIM2, 0u);
    }

    s_pulsing[axis] = 0u;
}

void Motor_StopAll(void) {
    /* 也供 fault 处理调用：Motor_Init 尚未跑过时直接返回，
       避免访问还没开时钟的 TIM2 再触发一次总线错误。 */
    if (s_initialized == 0u) {
        return;
    }

    if (Motor_PlotterOwnsTimer() != 0u) {
        Motor_PlotterEngineRequestStop();
        s_plot_preload_mask = 0u;
    }

    /* Emergency override: bypass public plotter-ownership guards. */
    TIM_SetCompare2(TIM2, 0u);
    TIM_SetCompare3(TIM2, 0u);
    s_pulsing[MOTOR_AXIS_X] = 0u;
    s_pulsing[MOTOR_AXIS_Y] = 0u;
}

/*
*********************************************************************************************************
*                                            状态查询
*********************************************************************************************************
*/
uint8_t Motor_IsPulsing(MotorAxis_t axis) {
    if (axis >= MOTOR_AXIS_COUNT) {
        return 0u;
    }
    return s_pulsing[axis];
}

uint32_t Motor_GetSteps(MotorAxis_t axis) {
    if (axis >= MOTOR_AXIS_COUNT) {
        return 0u;
    }
    return s_steps[axis];
}

void Motor_ResetSteps(MotorAxis_t axis) {
    if (Motor_PlotterOwnsTimer() != 0u) {
        return;
    }
    if (axis >= MOTOR_AXIS_COUNT) {
        return;
    }
    s_steps[axis] = 0u;
}

const char *Motor_AxisName(MotorAxis_t axis) {
    switch (axis) {
        case MOTOR_AXIS_X: return "X AXIS";
        case MOTOR_AXIS_Y: return "Y AXIS";
        default:           return "?";
    }
}

/*
*********************************************************************************************************
*                                    TIM2 更新中断：步数统计
*
*  1kHz 下每个更新事件正好对应一个完整 STEP 脉冲，所以在"正在出脉冲"期间
*  每个 UEV 记一步。中断服务程序在 stm32f4xx_it.c 里用
*  OSIntEnter()/OSIntExit() 包裹后调用本函数。
*********************************************************************************************************
*/
void Motor_UpdateISR(void) {
    uint32_t i;

    if (TIM_GetITStatus(TIM2, TIM_IT_Update) == RESET) {
        return;
    }
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

    /* Count only the plotter mask that occupied the period which just ended. */
    Motor_PlotterCountCompletedMask(s_plot_active_mask);
    s_plot_active_mask = s_plot_preload_mask;
    s_plot_preload_mask = 0u;
    Motor_PlotterScheduleISR();

    /* Preserve the original free-running jog accounting contract. */
    for (i = 0u; i < (uint32_t)MOTOR_AXIS_COUNT; i++) {
        if (s_pulsing[i] != 0u) {
            s_steps[i]++;
        }
    }
}
