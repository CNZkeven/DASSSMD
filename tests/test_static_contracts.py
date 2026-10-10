#!/usr/bin/env python3
"""Static regression checks for STM32F407 integration bugs.

These checks cover contracts that are difficult to exercise without hardware.
They intentionally inspect the C source for the failure-prone patterns described
in error.md.
"""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8", errors="ignore")


def body_of(source: str, name: str) -> str:
    match = re.search(rf"\b{name}\s*\([^)]*\)\s*\{{", source)
    assert match, f"{name}() not found"

    depth = 0
    for index in range(match.end() - 1, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[match.end():index]

    raise AssertionError(f"{name}() body is not balanced")


def int_macro(source: str, name: str) -> int:
    match = re.search(rf"^\s*#define\s+{name}\s+(\d+)\b", source, re.M)
    assert match, f"{name} macro not found"
    return int(match.group(1))


def uint_macro(source: str, name: str) -> int:
    match = re.search(rf"^\s*#define\s+{name}\s+(\d+)u?\b", source, re.M)
    assert match, f"{name} macro not found"
    return int(match.group(1))


def has_define(source: str, name: str) -> bool:
    return re.search(rf"^\s*#define\s+{name}\b", source, re.M) is not None


def c_code_without_comments(source: str) -> str:
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.S)


def test_touch_gpio_init_does_not_enable_exti_before_panel_init():
    source = read("User/touch/bsp_i2c_touch.c")

    init_body = body_of(source, "I2C_Touch_Init")

    assert "I2C_GTP_IRQEnable" not in init_body
    assert "I2C_GTP_IRQDisable" in init_body


def test_ui_task_runs_gtp_panel_initialization():
    source = read("User/ui/ui_task.c")

    ui_body = body_of(source, "UI_Task")
    uncommented = "\n".join(
        line.split("//", 1)[0]
        for line in ui_body.splitlines()
    )

    assert "GTP_Init_Panel();" in uncommented


def test_i2c_read_sends_ack_only_between_bytes_and_nack_after_final_byte():
    source = read("User/touch/bsp_i2c_touch.c")
    body = body_of(source, "I2C_ReadBytes")

    assert "if (bytes_remaining == 0)" in body
    assert body.index("*pBuffer = i2c_ReadByte();") < body.index("i2c_NAck();")
    assert body.index("i2c_NAck();") < body.index("i2c_Stop();")
    assert "else {\n            i2c_Ack();" in body


def test_i2c_transfer_returns_negative_errors_not_positive_success_counts():
    source = read("User/touch/gt9xx.c")
    body = body_of(source, "I2C_Transfer")

    assert "return -ret;" in body or "return -1;" in body
    assert "if(ret)\n\t\treturn ret;" not in body


def test_touch_ids_are_bounds_checked_before_track_arrays_are_indexed():
    source = read("User/touch/gt9xx.c")

    down_body = body_of(source, "GTP_Touch_Down")
    up_body = body_of(source, "GTP_Touch_Up")

    assert "id < 0 || id >= GTP_MAX_TOUCH" in down_body
    assert "id < 0 || id >= GTP_MAX_TOUCH" in up_body


def test_gtp_init_enables_irq_after_info_read_and_propagates_info_failure():
    source = read("User/touch/gt9xx.c")
    body = body_of(source, "GTP_Init_Panel")

    assert "ret = GTP_Get_Info();" in body
    assert "if (ret != SUCCESS)" in body
    assert "return -1;" in body
    assert body.index("ret = GTP_Get_Info();") < body.index("I2C_GTP_IRQEnable();")


def test_stm32f407_uses_25mhz_hse_pll_for_168mhz_system_clock():
    source = read("Libraries/CMSIS/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c")
    cmake = read("CMakeLists.txt")

    pllm_block = re.search(
        r"#if defined\(STM32F40_41xxx\).*?#define PLL_M\s+(\d+).*?#elif",
        source,
        re.S,
    )
    assert pllm_block, "STM32F40_41xxx PLL_M block not found"
    assert pllm_block.group(1) == "25"
    assert "HSE_VALUE=25000000" in cmake

    stm32f40_block = re.search(
        r"#if defined \(STM32F40_41xxx\)(.*?)#endif /\* STM32F40_41xxx \*/",
        source,
        re.S,
    )
    assert stm32f40_block, "STM32F40_41xxx PLL_N/PLL_P block not found"
    block = stm32f40_block.group(1)
    assert "#define PLL_N      336" in block
    assert "#define PLL_P      2" in block


def test_systick_init_uses_system_core_clock_not_literal_clock():
    source = read("User/main.c")

    assert "SystemCoreClockUpdate();" in source
    assert "OS_CPU_SysTickInit(SystemCoreClock / OS_CFG_TICK_RATE_HZ);" in source
    assert "OS_CPU_SysTickInit(168000)" not in source


def test_debug_usart_is_ready_before_tasks_can_log_and_sendbyte_has_timeout():
    main_source = read("User/main.c")
    usart_source = read("User/usart/bsp_usart.c")
    start_body = body_of(main_source, "AppTaskStart")
    send_body = body_of(usart_source, "Usart_SendByte")

    assert "Debug_USART_Config();" in start_body
    assert start_body.index("Debug_USART_Config();") < start_body.index("AppTaskCreate();")
    assert "timeout" in send_body
    assert "while (USART_GetFlagStatus(pUSARTx, USART_FLAG_TXE) == RESET);" not in send_body


def test_gtp_send_command_uses_command_argument():
    source = read("User/touch/gt9xx.c")
    body = body_of(source, "GTP_Send_Command")

    assert "command_buf[2] = command;" in body


# --------------------------------------------------------------------------- #
#                     电机点动页 / 长按运行 的集成契约                          #
# --------------------------------------------------------------------------- #


def test_tim2_isr_is_kernel_aware_and_only_counts():
    body = body_of(read("User/stm32f4xx_it.c"), "TIM2_IRQHandler")

    assert "OSIntEnter" in body
    assert "OSIntExit" in body
    assert "Motor_UpdateISR" in body
    # 中断里不做重活：不能延时、不能打印、不能画屏
    assert "OSTimeDly" not in body
    assert "printf" not in body
    assert "UI_Draw" not in body


def test_fault_handler_stops_motors_before_spinning():
    """fault 里的 while(1) 不会停掉硬件 PWM。

    TIM2 只要 CCR 还非零就会一直出脉冲，机构会自己走到撞限位。
    所以死循环之前必须先 Motor_StopAll()。
    """
    body = body_of(read("User/stm32f4xx_it.c"), "Fault_Indicate")

    assert "Motor_StopAll" in body
    assert body.index("Motor_StopAll") < body.index("while (1)")


def test_motor_init_leaves_ccr_at_zero():
    """上电必须静止：CCR 初值为 0，PWM1 模式下引脚恒低。"""
    init_body = body_of(read("User/motor/bsp_motor.c"), "Motor_Init")

    assert re.search(r"TIM_Pulse\s*=\s*0u", init_body)
    assert "TIM_Cmd(TIM2, ENABLE)" in init_body


def test_direction_is_established_before_pulses_start():
    """DIR 必须先建立再出脉冲，否则驱动器会丢步甚至走反一步。"""
    body = body_of(read("User/task/motor_task.c"), "Jog_Update")

    assert "Motor_SetDir" in body
    assert "Motor_StartPulse" in body
    assert "MOTOR_DIR_SETUP_TICKS" in body
    assert body.index("Motor_SetDir") < body.index("Motor_StartPulse")


def test_keys_stay_disarmed_until_released_after_boot():
    """上电时若某个键已经被按住，必须先松开一次才允许点动。"""
    key_c = read("User/key/bsp_key.c")

    assert "s_armed" in key_c
    assert "KEY_DEBOUNCE_MS" in read("User/key/bsp_key.h")
    assert "s_armed" in body_of(key_c, "Key_IsHeld")


def test_axis_switch_is_refused_while_running():
    body = body_of(read("User/ui/ui_task.c"), "UI_HandleMotorTouch")

    assert "Motor_JogIsRunning" in body
    assert "Motor_SelectAxis" in body
    assert body.index("Motor_JogIsRunning") < body.index("Motor_SelectAxis")


def test_openocd_keeps_connect_under_reset():
    """PA14 = Y 轴 DIR，和 SWCLK 冲突。

    丢掉 connect_assert_srst 就再也连不上芯片了 —— 这是把板子变砖的那一步。
    注释里提到旧配置不算数，这里只看生效的指令。
    """
    cfg = read("openocd.cfg")
    directives = "\n".join(
        line for line in cfg.splitlines() if not line.lstrip().startswith("#")
    )

    assert "connect_assert_srst" in directives
    assert "none separate" not in directives


def test_motor_sources_are_wired_into_the_build():
    cmake = read("CMakeLists.txt")

    for src in (
        "User/motor/bsp_motor.c",
        "User/key/bsp_key.c",
        "User/task/motor_task.c",
    ):
        assert src in cmake, f"{src} 没有加入构建"

    assert "${CMAKE_SOURCE_DIR}/User/motor" in cmake
    assert "${CMAKE_SOURCE_DIR}/User/key" in cmake


def test_plotter_core_is_built_and_usb_demos_stay_excluded():
    cmake = read("CMakeLists.txt")

    include_block = re.search(
        r"include_directories\(\n(?P<body>.*?)\n\)",
        cmake,
        re.S,
    )
    assert include_block, "include_directories block not found"
    assert "${CMAKE_SOURCE_DIR}/User/plotter" in include_block.group("body")

    user_sources = re.search(
        r"set\(USER_SOURCES\n(?P<body>.*?)\n\)",
        cmake,
        re.S,
    )
    assert user_sources, "USER_SOURCES block not found"
    user_body = user_sources.group("body")
    assert "${CMAKE_SOURCE_DIR}/User/plotter/plotter_path.c" in user_body

    for source_list in ("USER_SOURCES", "CMSIS_SOURCES", "PERIPH_SOURCES", "UCOS_SOURCES"):
        block = re.search(
            rf"set\({source_list}\n(?P<body>.*?)\n\)",
            cmake,
            re.S,
        )
        assert block, f"{source_list} block not found"
        active_body = block.group("body")
        assert not re.search(r"(?i)usb", active_body), (
            f"USB demo source leaked into active {source_list}"
        )

    executable = re.search(
        r"add_executable\(\$\{PROJECT_NAME\}\.elf\n(?P<body>.*?)\n\)",
        cmake,
        re.S,
    )
    assert executable, "firmware add_executable block not found"
    assert executable.group("body").split() == [
        "${USER_SOURCES}",
        "${CMSIS_SOURCES}",
        "${PERIPH_SOURCES}",
        "${UCOS_SOURCES}",
    ]


# --------------------------------------------------------------------------- #
#                    精简之后：只剩电机程序这一条约束链                            #
# --------------------------------------------------------------------------- #

DEMO_SOURCES = (
    "User/adc/bsp_adc.c",
    "User/beep/bsp_beep.c",
    "User/led/bsp_led.c",
    "User/task/industrial_temp.c",
    "User/task/worker_task.c",
)

DEMO_TASK_IDS = (
    "TASK_BUZZER",
    "TASK_UART",
    "TASK_ADC",
    "TASK_GPIO_LED",
    "TASK_ADC_LED",
    "TASK_INDUSTRIAL_TEMP",
)


def test_demo_programs_are_gone():
    cmake = read("CMakeLists.txt")

    for src in DEMO_SOURCES:
        assert not (ROOT / src).exists(), f"{src} 应该已经删掉"
        assert src not in cmake, f"{src} 还留在构建里"


def test_only_two_application_tasks_are_created():
    app_cfg = read("User/app_cfg.h")
    main_c = read("User/main.c")

    # 任务优先级只剩电机和 UI 两个
    assert has_define(app_cfg, "MOTOR_TASK_PRIO")
    assert has_define(app_cfg, "UI_TASK_PRIO")
    assert not has_define(app_cfg, "WORKER_TASK_PRIO")

    # main.c 里也只创建这两个
    assert "Motor_Task" in main_c
    assert "UI_Task" in main_c
    assert "Worker_Task" not in main_c
    assert "OSStart" in main_c


def test_motor_task_outranks_ui():
    app_cfg = read("User/app_cfg.h")

    motor = int_macro(app_cfg, "MOTOR_TASK_PRIO")
    ui = int_macro(app_cfg, "UI_TASK_PRIO")

    # uCOS 里数值越小优先级越高。电机任务必须比 UI 高：
    # 松开按键就是要停，不能排在画屏和触摸屏软件 I2C 后面。
    assert motor < ui, f"MOTOR={motor} UI={ui}"

    # 也不能高到撞上内核自己用的优先级（tick 任务 = 1，启动任务 = 2）
    assert motor > 2, f"MOTOR={motor} 会撞上 uCOS 内核任务的优先级"


def test_no_leftover_task_id_enum_or_menu():
    """单页面之后 TaskID_t / 主菜单 / 返回按钮都不该再存在。"""
    app_cfg = read("User/app_cfg.h")
    ui_task = read("User/ui/ui_task.c")
    ui_draw = read("User/ui/ui_draw.c")

    assert "TaskID_t" not in app_cfg
    assert "g_current_task" not in ui_task
    assert "g_current_task" not in ui_draw

    assert "task_buttons" not in ui_draw
    assert "UI_DrawMainMenu" not in ui_draw
    assert "UI_DrawBackButton" not in ui_draw
    assert "UI_DrawTaskView" not in ui_draw

    for task_id in DEMO_TASK_IDS:
        assert task_id not in app_cfg, f"{task_id} 还留在 app_cfg.h"


def test_motor_page_is_the_only_page_and_is_drawn_at_boot():
    body = body_of(read("User/ui/ui_task.c"), "UI_Task")

    assert "UI_DrawMotorView" in body
    # LCD 必须先初始化，再画
    assert body.index("NT35510_Init") < body.index("UI_DrawMotorView")
    # 触摸屏也要先起来，否则切轴按钮点不动
    assert body.index("GTP_Init_Panel") < body.index("UI_DrawMotorView")


def test_ui_draw_only_exposes_the_motor_page():
    header = read("User/ui/ui_draw.h")

    assert "UI_DrawMotorView" in header
    assert "UI_DrawMotorState" in header
    assert "UI_DrawMotorStatus" in header

    for gone in ("UI_DrawAdcValues", "UI_DrawIndustrialTempView", "Button_t"):
        assert gone not in header, f"{gone} 应该已经从 ui_draw.h 删掉"


def test_gt9xx_exports_down_move_up_multi_and_error_events():
    header = read("User/touch/gt9xx.h")
    source = read("User/touch/gt9xx.c")

    for name, value in (
        ("GTP_EVENT_NONE", "0"),
        ("GTP_EVENT_DOWN", None),
        ("GTP_EVENT_MOVE", None),
        ("GTP_EVENT_UP", None),
        ("GTP_EVENT_MULTI", None),
        ("GTP_EVENT_ERROR", None),
    ):
        assert name in header
        if value is not None:
            assert re.search(rf"{name}\s*=\s*{value}", header)

    event_struct = re.search(
        r"typedef\s+struct\s*\{(?P<body>.*?)\}\s*GTP_TouchEvent_t\s*;",
        header,
        re.S,
    )
    assert event_struct, "GTP_TouchEvent_t not found"
    for declaration in (
        "GTP_TouchEventType_t type;",
        "uint16_t x;",
        "uint16_t y;",
        "uint8_t touch_count;",
    ):
        assert declaration in event_struct.group("body")

    assert re.search(
        r"void\s+GTP_TouchProcess\s*\(\s*GTP_TouchEvent_t\s*\*\s*event\s*\)",
        header,
    )

    down = body_of(source, "GTP_Touch_Down")
    up = body_of(source, "GTP_Touch_Up")
    process = body_of(source, "GTP_TouchProcess")
    worker = body_of(source, "Goodix_TS_Work_Func")
    get_point = body_of(source, "GTP_GetTouchPoint")

    assert "GTP_EVENT_DOWN" in down
    assert "GTP_EVENT_MOVE" in down
    assert "GTP_EVENT_UP" in up
    assert "GTP_EVENT_MULTI" in worker
    assert "touch_num > 1" in worker
    assert "g_touch_ambiguous" in worker

    # Initial read, extra-coordinate read, and end-command write must each
    # reject every short/failed transfer result and fail closed.
    assert worker.count("GTP_EVENT_ERROR") >= 3
    assert worker.count("ret != 2") == 2
    assert "ret != 1" in worker
    assert worker.count("GTP_Touch_ResetState()") >= 3

    # Both no-fresh-report cases retain the initialized NONE event.
    assert "event->type = GTP_EVENT_NONE;" in worker
    assert re.search(r"if\s*\(finger\s*==\s*0x00\)\s*\{[^}]*return;", worker, re.S)
    assert re.search(r"if\s*\(\(finger\s*&\s*0x80\)\s*==\s*0\).*?goto exit_work_func;", worker, re.S)
    assert "event->type = GTP_EVENT_NONE;" in process
    assert "event->x = 0;" in process and "event->y = 0;" in process
    assert "g_touch_flag = 1;" in down
    assert "g_touch_flag = 0;" in up
    assert "*x = g_touch_x;" in get_point and "*y = g_touch_y;" in get_point

    extraction = process + worker
    assert not re.search(r"\b(?:LCD_[A-Za-z0-9_]+|NT35510_[A-Za-z0-9_]+)\s*\(", extraction)
    assert not re.search(r"\b(?:OSTime[A-Za-z0-9_]*|Motor_[A-Za-z0-9_]*)\s*\(", extraction)
    assert "Touch_Button" not in extraction
    assert "Draw_Trail" not in extraction


def test_motor_plotter_uses_bounded_timer_dda_and_no_blocking_calls():
    header = read("User/motor/bsp_motor.h")
    source = read("User/motor/bsp_motor.c")

    assert re.search(r"#define\s+PLOTTER_STEP_RATE_HZ\s+1000u\b", header)
    for state in (
        "MOTOR_PLOT_ENGINE_IDLE",
        "MOTOR_PLOT_ENGINE_RUNNING",
        "MOTOR_PLOT_ENGINE_STOPPING",
        "MOTOR_PLOT_ENGINE_DONE",
        "MOTOR_PLOT_ENGINE_ABORTED",
    ):
        assert state in header
    for declaration in (
        "Motor_PlotterEngineStart(const PlotterPath_t *path)",
        "Motor_PlotterEngineRequestStop(void)",
        "Motor_PlotterEngineGetState(void)",
        "Motor_PlotterEngineGetProgress(void)",
    ):
        assert declaration in header

    start_body = body_of(source, "Motor_PlotterEngineStart")
    assert "PLOTTER_PATH_READY" in start_body
    assert "path->count < 2u" in start_body
    assert "path->count > PLOTTER_MAX_POINTS" in start_body
    assert "s_pulsing" in start_body
    assert "PlotterDda_Begin(&s_plot_dda, path)" in start_body
    assert "Motor_PlotterMeasurePath(path" in start_body
    assert "s_plot_total_major_steps" in start_body
    assert start_body.index("s_plot_state = MOTOR_PLOT_ENGINE_RUNNING") > start_body.index(
        "s_plot_total_major_steps"
    )

    isr_body = body_of(source, "Motor_UpdateISR")
    assert isr_body.index("TIM_GetITStatus(TIM2, TIM_IT_Update)") < isr_body.index(
        "TIM_ClearITPendingBit(TIM2, TIM_IT_Update)"
    )
    assert "Motor_PlotterCountCompletedMask(s_plot_active_mask)" in isr_body
    assert isr_body.index("Motor_PlotterCountCompletedMask(s_plot_active_mask)") < isr_body.index(
        "s_plot_active_mask = s_plot_preload_mask"
    )
    assert "Motor_PlotterScheduleISR()" in isr_body

    schedule_body = body_of(source, "Motor_PlotterScheduleISR")
    assert "PLOTTER_STEP_RATE_HZ" in schedule_body
    assert "s_plot_rate_phase" in schedule_body
    assert schedule_body.count("PlotterDda_Next(") == 1
    assert "MOTOR_PLOT_ENGINE_STOPPING" in schedule_body
    assert "MOTOR_PLOT_ENGINE_ABORTED" in schedule_body
    assert "MOTOR_PLOT_ENGINE_DONE" in schedule_body
    assert "s_plot_preload_mask = 0u" in schedule_body
    assert "s_plot_active_mask == 0u" in schedule_body
    uncommented_schedule = c_code_without_comments(schedule_body)
    assert re.search(
        r"if \(s_plot_state != MOTOR_PLOT_ENGINE_RUNNING\) \{\s*return;",
        uncommented_schedule,
    ), "inactive plotter states must not overwrite the jog CCRs"

    output_body = body_of(source, "Motor_PlotterWriteCompareMask")
    assert "TIM_SetCompare2(TIM2," in output_body
    assert "TIM_SetCompare3(TIM2," in output_body
    assert "PLOTTER_AXIS_X" in output_body
    assert "PLOTTER_AXIS_Y" in output_body
    assert "MOTOR_PULSE_CCR" in output_body
    assert re.search(
        r"else\s*\{\s*TIM_SetCompare2\(TIM2,\s*0u\);", output_body
    )
    assert re.search(
        r"else\s*\{\s*TIM_SetCompare3\(TIM2,\s*0u\);", output_body
    )

    pure_scheduler_bodies = (
        isr_body,
        schedule_body,
        body_of(source, "Motor_PlotterCountCompletedMask"),
        output_body,
    )
    forbidden_calls = re.compile(r"OSTimeDly|printf|NT35510_|I2C_")
    for body in pure_scheduler_bodies:
        assert not forbidden_calls.search(c_code_without_comments(body))


def test_plotter_engine_exclusively_owns_jog_motion_controls():
    source = read("User/motor/bsp_motor.c")

    owns_timer = body_of(source, "Motor_PlotterOwnsTimer")
    assert "MOTOR_PLOT_ENGINE_RUNNING" in owns_timer
    assert "MOTOR_PLOT_ENGINE_STOPPING" in owns_timer

    guard = re.compile(
        r"if\s*\(\s*Motor_PlotterOwnsTimer\(\)\s*!=\s*0u\s*\)\s*\{\s*return;\s*\}"
    )
    for name in (
        "Motor_SetDir",
        "Motor_StartPulse",
        "Motor_StopPulse",
        "Motor_ResetSteps",
    ):
        body = c_code_without_comments(body_of(source, name))
        assert guard.search(body), f"{name} must return before mutating plotter-owned state"

    set_dir_body = c_code_without_comments(body_of(source, "Motor_SetDir"))
    scheduler_body = c_code_without_comments(body_of(source, "Motor_PlotterScheduleISR"))
    assert "Motor_SetDirInternal" in set_dir_body
    assert "Motor_SetDirInternal" in scheduler_body
    assert "Motor_SetDir(" not in scheduler_body

    stop_all_body = c_code_without_comments(body_of(source, "Motor_StopAll"))
    assert "Motor_PlotterEngineRequestStop()" in stop_all_body
    assert re.search(r"TIM_SetCompare2\(TIM2,\s*0u\)", stop_all_body)
    assert re.search(r"TIM_SetCompare3\(TIM2,\s*0u\)", stop_all_body)
    assert "s_pulsing[MOTOR_AXIS_X] = 0u" in stop_all_body
    assert "s_pulsing[MOTOR_AXIS_Y] = 0u" in stop_all_body


def test_motor_task_serializes_jog_and_plotter_lifecycle():
    header = read("User/task/motor_task.h")
    source = read("User/task/motor_task.c")
    uncommented_header = c_code_without_comments(header)
    uncommented_source = c_code_without_comments(source)

    assert '#include "bsp_motor.h"' in uncommented_header
    assert '#include "plotter_path.h"' not in uncommented_header

    plot_state = re.search(
        r"typedef\s+enum\s*\{(?P<body>.*?)\}\s*MotorPlotState_t\s*;",
        uncommented_header,
        re.S,
    )
    assert plot_state, "MotorPlotState_t must be public in motor_task.h"
    for state in (
        "MOTOR_PLOT_IDLE",
        "MOTOR_PLOT_PENDING",
        "MOTOR_PLOT_RUNNING",
        "MOTOR_PLOT_DONE",
        "MOTOR_PLOT_ABORTED",
        "MOTOR_PLOT_REJECTED",
    ):
        assert state in plot_state.group("body")

    for declaration in (
        "Motor_PlotterRequestStart(const PlotterPath_t *path)",
        "Motor_PlotterRequestStop(void)",
        "Motor_PlotterGetState(void)",
        "Motor_PlotterGetProgress(void)",
    ):
        assert declaration in uncommented_header

    start_body = c_code_without_comments(body_of(source, "Motor_PlotterRequestStart"))
    assert "path == (const PlotterPath_t *)0" in start_body
    assert "path->state != PLOTTER_PATH_READY" in start_body
    assert "path->count < 2u" in start_body
    assert "path->count > PLOTTER_MAX_POINTS" in start_body
    assert "MOTOR_PLOT_IDLE" in start_body
    assert "MOTOR_PLOT_DONE" in start_body
    assert "MOTOR_PLOT_ABORTED" in start_body
    assert "CPU_SR_ALLOC()" in start_body
    assert "CPU_CRITICAL_ENTER()" in start_body
    assert "CPU_CRITICAL_EXIT()" in start_body
    assert start_body.index("CPU_CRITICAL_ENTER()") < start_body.index("s_plot_path = path")
    assert start_body.index("s_plot_path = path") < start_body.index(
        "s_plot_task_state = MOTOR_PLOT_PENDING"
    ) < start_body.index("CPU_CRITICAL_EXIT()")
    assert "Motor_PlotterEngineStart(" not in start_body

    stop_body = c_code_without_comments(body_of(source, "Motor_PlotterRequestStop"))
    assert "s_plot_stop_requested = 1u" in stop_body
    assert "Motor_PlotterEngineRequestStop()" not in stop_body

    progress_body = c_code_without_comments(body_of(source, "Motor_PlotterGetProgress"))
    assert "MOTOR_PLOT_DONE" in progress_body
    assert "return 100u" in progress_body
    assert "Motor_PlotterEngineGetProgress()" in progress_body

    publish_body = c_code_without_comments(body_of(source, "Plotter_PublishTerminal"))
    assert "CPU_CRITICAL_ENTER()" in publish_body
    assert "CPU_CRITICAL_EXIT()" in publish_body
    assert publish_body.index("s_plot_task_state = terminal_state") < publish_body.index(
        "s_plot_path = (const PlotterPath_t *)0"
    )
    assert "s_plot_pending" in publish_body or "s_plot_stop_requested = 0u" in publish_body
    assert "s_plot_stop_requested = 0u" in publish_body

    update_body = c_code_without_comments(body_of(source, "Plotter_Update"))
    assert "s_state == JOG_IDLE" in update_body
    assert "s_running == 0u" in update_body
    assert "plot_owned_cycle = 1u" in update_body
    assert "Key_IsAnyHeld()" in update_body
    assert "PLOTTER_PATH_READY" in update_body
    assert "Motor_PlotterEngineStart(path)" in update_body
    assert "Motor_PlotterEngineRequestStop()" in update_body
    assert "MOTOR_PLOT_ENGINE_RUNNING" in update_body
    assert "MOTOR_PLOT_ENGINE_STOPPING" in update_body
    assert "MOTOR_PLOT_ENGINE_DONE" in update_body
    assert "MOTOR_PLOT_ENGINE_ABORTED" in update_body
    assert "MOTOR_PLOT_ENGINE_IDLE" in update_body
    assert "MOTOR_PLOT_REJECTED" in update_body
    assert "MOTOR_PLOT_ABORTED" in update_body
    assert "s_plot_jog_inhibit = 1u" in update_body
    assert re.search(
        r"if\s*\(\s*s_plot_task_state\s*!=\s*MOTOR_PLOT_PENDING\s*&&\s*"
        r"s_plot_task_state\s*!=\s*MOTOR_PLOT_RUNNING\s*&&\s*"
        r"s_plot_jog_inhibit\s*!=\s*0u\s*&&\s*Key_IsAnyHeld\(\)\s*==\s*0u\s*\)\s*\{\s*"
        r"s_plot_jog_inhibit\s*=\s*0u\s*;",
        update_body,
    ), "jog inhibit clears only after playback is terminal and both keys are released"

    task_body = c_code_without_comments(body_of(source, "Motor_Task"))
    assert task_body.index("Key_Scan()") < task_body.index("Plotter_Update()")
    assert task_body.index("Plotter_Update()") < task_body.index("Jog_Update()")
    assert re.search(
        r"if\s*\(\s*Plotter_Update\(\)\s*==\s*0u\s*&&\s*"
        r"s_plot_jog_inhibit\s*==\s*0u\s*\)\s*\{\s*Jog_Update\(\);",
        task_body,
    ), "pending/running playback and the post-plot key inhibit must suppress jog"


def test_rejected_plotter_request_can_be_retried_without_reset():
    source = read("User/task/motor_task.c")
    start_body = c_code_without_comments(body_of(source, "Motor_PlotterRequestStart"))

    assert "MOTOR_PLOT_REJECTED" in start_body, (
        "a rejected plot must be retryable after the user fixes the rejection cause"
    )


def test_pending_plot_request_inhibits_jog_before_start_rejection():
    source = read("User/task/motor_task.c")
    update_body = c_code_without_comments(body_of(source, "Plotter_Update"))
    pending_start = update_body.index("if (task_state == MOTOR_PLOT_PENDING)")
    running_start = update_body.index(
        "} else if (task_state == MOTOR_PLOT_RUNNING)", pending_start
    )
    pending_body = update_body[pending_start:running_start]

    inhibit_at = pending_body.find("s_plot_jog_inhibit = 1u")
    assert inhibit_at >= 0, "consuming PENDING must inhibit jogging"
    assert inhibit_at < pending_body.index("if (stop_requested != 0u)")
    assert inhibit_at < pending_body.index("can_start ="), (
        "pending jog inhibit must precede can_start checks that may reject the request"
    )


def test_pending_stop_wins_over_eligibility_rejection():
    source = read("User/task/motor_task.c")
    terminal_body = c_code_without_comments(body_of(source, "Plotter_PublishTerminal"))
    update_body = c_code_without_comments(body_of(source, "Plotter_Update"))

    assert terminal_body.count("CPU_CRITICAL_ENTER()") == 1
    assert terminal_body.count("CPU_CRITICAL_EXIT()") == 1
    assert re.search(
        r"if\s*\(\s*terminal_state\s*==\s*MOTOR_PLOT_REJECTED\s*&&\s*"
        r"s_plot_task_state\s*==\s*MOTOR_PLOT_PENDING\s*&&\s*"
        r"s_plot_stop_requested\s*!=\s*0u\s*\)",
        terminal_body,
    ), "terminal publication must arbitrate a pending STOP against rejection"
    assert "terminal_state = MOTOR_PLOT_ABORTED;" in terminal_body
    assert terminal_body.index("CPU_CRITICAL_ENTER()") < terminal_body.index(
        "terminal_state = MOTOR_PLOT_ABORTED;"
    ) < terminal_body.index("s_plot_task_state = terminal_state")
    assert terminal_body.index("s_plot_task_state = terminal_state") < terminal_body.index(
        "s_plot_path = (const PlotterPath_t *)0"
    ) < terminal_body.index("s_plot_stop_requested = 0u") < terminal_body.index(
        "CPU_CRITICAL_EXIT()"
    )
    assert re.search(
        r"if\s*\(can_start\s*==\s*0u\s*\)\s*\{\s*"
        r"Plotter_PublishTerminal\(MOTOR_PLOT_REJECTED\);",
        update_body,
    ), "eligibility failure must route through atomic terminal arbitration"


def test_ui_draw_flow_requires_center_valid_path_and_start():
    task_source = read("User/ui/ui_task.c")
    task_header = read("User/ui/ui_task.h")
    draw_source = read("User/ui/ui_draw.c")
    draw_header = read("User/ui/ui_draw.h")
    touch_header = read("User/touch/gt9xx.h")
    plotter_header = read("User/plotter/plotter_path.h")

    state_enum = re.search(
        r"typedef\s+enum\s*\{(?P<body>.*?)\}\s*UI_State_t\s*;",
        task_header,
        re.S,
    )
    assert state_enum, "UI_State_t must include the one-stroke drawing lifecycle"
    for state in (
        "UI_PAGE_MOTOR",
        "UI_DRAW_CENTER_CONFIRM",
        "UI_DRAW_READY",
        "UI_DRAW_CAPTURING",
        "UI_DRAW_PREVIEW",
        "UI_DRAW_INVALID",
        "UI_DRAW_PLAYING",
        "UI_DRAW_RESULT",
    ):
        assert state in state_enum.group("body")

    motor_view = body_of(draw_source, "UI_DrawMotorView")
    motor_touch = body_of(task_source, "UI_HandleMotorTouch")
    assert "DRAW" in motor_view
    assert "UI_DRAW_BUTTON" in motor_touch
    assert "Motor_JogIsRunning()" in motor_touch
    assert "MOTOR_PLOT_PENDING" in motor_touch
    assert "MOTOR_PLOT_RUNNING" in motor_touch

    assert re.search(
        r"void\s+GTP_TouchProcess\s*\(\s*GTP_TouchEvent_t\s*\*\s*event\s*\)",
        touch_header,
    )
    task_body = body_of(task_source, "UI_Task")
    assert "GTP_TouchEvent_t event" in task_body
    assert "GTP_EVENT_NONE" in task_body
    assert re.search(r"if\s*\(\s*g_gtp_irq_flag(?:\s*!=\s*0u)?\s*\)", task_body)
    assert "GTP_TouchProcess(&event)" in task_body
    assert "UI_HandleDrawEvent(&event)" in task_body

    confirm_body = body_of(task_source, "UI_ConfirmCenter")
    assert "PlotterPath_Begin" in confirm_body
    assert confirm_body.index("PlotterPath_Begin") < confirm_body.index("UI_DRAW_READY")

    event_body = body_of(task_source, "UI_HandleDrawEvent")
    for event_name in (
        "GTP_EVENT_DOWN",
        "GTP_EVENT_MOVE",
        "GTP_EVENT_UP",
        "GTP_EVENT_MULTI",
        "GTP_EVENT_ERROR",
    ):
        assert event_name in event_body
    assert "PlotterPath_AppendTouch" in task_source
    assert "PlotterPath_Finish" in event_body
    assert "UI_DRAW_CAPTURING" in event_body
    assert "UI_DRAW_INVALID" in event_body
    assert "PLOTTER_PATH_OUT_OF_BOUNDS" in task_source
    assert "PLOTTER_PATH_OVERFLOW" in task_source

    start_body = body_of(task_source, "UI_StartPlayback")
    assert "UI_DRAW_PREVIEW" in start_body
    assert "PLOTTER_PATH_READY" in start_body
    assert "Motor_PlotterRequestStart(&s_plotter_path)" in start_body
    assert "Motor_PlotterRequestStart" not in event_body
    stop_body = body_of(task_source, "UI_RequestPlotterStop")
    assert "Motor_PlotterRequestStop()" in stop_body
    assert "Motor_PlotterRequestStop" not in task_body

    update_body = body_of(task_source, "UI_UpdatePlotterState")
    assert "MOTOR_PLOT_REJECTED" in update_body
    assert "MOTOR_PLOT_DONE" in update_body
    assert "MOTOR_PLOT_ABORTED" in update_body
    assert "s_center_ready = 0u" in update_body
    assert "PLOTTER_PATH_READY" in update_body

    motor_branch = re.search(
        r"if\s*\(s_ui_state\s*==\s*UI_PAGE_MOTOR\)\s*\{(?P<body>.*?)\}\s*else\s*\{",
        task_body,
        re.S,
    )
    assert motor_branch, "motor and drawing task loops must be separate"
    assert "UI_HandleTouch()" in motor_branch.group("body")
    assert "UI_MotorRefresh()" in motor_branch.group("body")
    assert "OSTimeDly(50" in motor_branch.group("body")
    draw_branch = task_body[motor_branch.end():]
    assert "OSTimeDly(10" in draw_branch

    labels = re.findall(
        r"\bUI_DrawString(?:Bounded)?\s*\([^;]*?\"((?:\\.|[^\"\\])*)\"",
        c_code_without_comments(draw_source),
        re.S,
    )
    assert labels, "drawing labels should be passed through the bounded UI helpers"
    assert all(label.isascii() for label in labels), "LCD labels must remain ASCII"

    for name, value in (
        ("UI_PLOT_CANVAS_LEFT", "24"),
        ("UI_PLOT_CANVAS_TOP", "220"),
        ("UI_PLOT_CANVAS_WIDTH", "433"),
        ("UI_PLOT_CANVAS_HEIGHT", "285"),
        ("UI_PLOT_CENTER_X", "240"),
        ("UI_PLOT_CENTER_Y", "362"),
    ):
        assert re.search(rf"#define\s+{name}\s+{value}u?\b", draw_header)
    assert "PLOTTER_X_LIMIT_STEPS" in draw_source
    assert "PLOTTER_Y_LIMIT_STEPS" in draw_source
    assert re.search(r"#define\s+PLOTTER_X_LIMIT_STEPS\s+3500\b", plotter_header)
    assert re.search(r"#define\s+PLOTTER_Y_LIMIT_STEPS\s+2300\b", plotter_header)


def test_plotter_task_publishes_running_only_after_engine_accepts_start():
    source = read("User/task/motor_task.c")
    body = c_code_without_comments(body_of(source, "Plotter_Update"))

    engine_call = body.index("Motor_PlotterEngineStart(path)")
    running_publish = body.index("s_plot_task_state = MOTOR_PLOT_RUNNING")
    assert engine_call < running_publish, (
        "UI must not consume center confirmation from a transient RUNNING state "
        "before the plot engine accepts the path"
    )


def test_ui_stop_status_does_not_force_full_page_redraw_each_tick():
    source = read("User/ui/ui_task.c")
    body = c_code_without_comments(body_of(source, "UI_UpdatePlotterState"))

    running_branch = body[body.index("if (plot_state == MOTOR_PLOT_RUNNING)"):]
    running_branch = running_branch[:running_branch.index("if (plot_state == MOTOR_PLOT_REJECTED)")]
    redraw_condition = re.search(r"if\s*\((?P<condition>.*?)\)\s*\{", running_branch, re.S)
    assert redraw_condition, "RUNNING view redraw condition not found"
    assert "s_stop_requested" not in redraw_condition.group("condition"), (
        "a persistent stop flag must not force full-page redraw on every 10ms tick"
    )


def test_ambiguous_touch_state_resets_after_all_contacts_lift():
    source = read("User/touch/gt9xx.c")
    worker = c_code_without_comments(body_of(source, "Goodix_TS_Work_Func"))

    assert re.search(
        r"if\s*\(\s*touch_num\s*==\s*0u?\s*\)\s*\{.*?"
        r"GTP_Touch_ResetState\(\)\s*;",
        worker,
        re.S,
    ), "ambiguous touch state must be cleared after the final contact lifts"


# Outside-canvas DOWN and clamped MOVE behavior is exercised by
# test_drawing_ui_host.py using the real UI state machine.


def test_no_stale_no_argument_gtp_process_declaration_in_isr_file():
    source = read("User/stm32f4xx_it.c")
    assert "extern void GTP_TouchProcess(void)" not in source


def test_readme_and_flash_hint_match_page_cadence_and_build_invocation():
    readme = read("README.md")
    flash = read("flash.sh")

    assert re.search(r"UI_Task\s+prio\s+4：电机页 50 ms；作画/播放页 10 ms", readme)
    assert "bash build.sh" in readme
    assert "bash flash.sh" in readme
    assert "请先编译：bash build.sh" in flash
    assert "请先编译：./build.sh" not in flash
    assert "PLOTTER_STEP_RATE_HZ = 1000" in readme
