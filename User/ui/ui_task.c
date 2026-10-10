#include "ui_task.h"
#include "ui_draw.h"
#include "bsp_nt35510_lcd.h"
#include "bsp_i2c_touch.h"
#include "gt9xx.h"
#include "motor_task.h"
#include "plotter_path.h"
#include "plotter_demo.h"
#include "boot_trace.h"
#include "os.h"
#include <stddef.h>

#define TOUCH_DEBOUNCE_MS  200u

extern volatile uint8_t g_gtp_irq_flag;  /* Set by the touch ISR; I2C stays in task context. */

static uint32_t last_touch_time = 0u;

static UI_State_t s_ui_state = UI_PAGE_MOTOR;
static PlotterPath_t s_plotter_path;
static const PlotterCanvas_t s_plotter_canvas = {
    UI_PLOT_CANVAS_LEFT,
    UI_PLOT_CANVAS_TOP,
    UI_PLOT_CANVAS_WIDTH,
    UI_PLOT_CANVAS_HEIGHT
};
static uint8_t s_center_ready = 0u;
static uint8_t s_motion_accepted = 0u;
static uint8_t s_stop_requested = 0u;
static uint8_t s_play_view_valid = 0u;
static uint16_t s_last_progress = 0u;
static uint16_t s_last_touch_x = 0u;
static uint16_t s_last_touch_y = 0u;
static uint8_t s_demo_page = 0u;
static uint8_t s_demo_selected = 0u;
static PlotterDemoId_t s_selected_demo = PLOTTER_DEMO_FLOWER;
static MotorPlotState_t s_last_plot_state = MOTOR_PLOT_IDLE;
static const char *s_invalid_reason = "INVALID STROKE";

typedef struct {
    MotorAxis_t axis;
    uint8_t     running;
    MotorDir_t  dir;
    uint32_t    steps;
} MotorSnapshot_t;

static MotorSnapshot_t motor_snapshot;
static uint8_t         motor_snapshot_valid = 0u;

static void UI_MotorSnapshotCapture(void) {
    motor_snapshot.axis    = Motor_GetSelectedAxis();
    motor_snapshot.running = Motor_JogIsRunning();
    motor_snapshot.dir     = Motor_JogGetDir();
    motor_snapshot.steps   = Motor_GetSteps(motor_snapshot.axis);
    motor_snapshot_valid   = 1u;
}

static void UI_MotorRefresh(void) {
    MotorSnapshot_t now;

    now.axis    = Motor_GetSelectedAxis();
    now.running = Motor_JogIsRunning();
    now.dir     = Motor_JogGetDir();
    now.steps   = Motor_GetSteps(now.axis);

    if ((motor_snapshot_valid != 0u) &&
        (now.axis    == motor_snapshot.axis) &&
        (now.running == motor_snapshot.running) &&
        (now.dir     == motor_snapshot.dir) &&
        (now.steps   == motor_snapshot.steps)) {
        return;
    }

    motor_snapshot       = now;
    motor_snapshot_valid = 1u;
    UI_DrawMotorStatus();
}

static uint8_t UI_TouchInRect(uint16_t x,
                              uint16_t y,
                              uint16_t left,
                              uint16_t top,
                              uint16_t width,
                              uint16_t height) {
    uint32_t right = (uint32_t)left + width;
    uint32_t bottom = (uint32_t)top + height;

    return (uint8_t)(x >= left && (uint32_t)x < right &&
                     y >= top && (uint32_t)y < bottom);
}

static uint8_t UI_TouchInCanvas(uint16_t x, uint16_t y) {
    return UI_TouchInRect(x, y,
                          UI_PLOT_CANVAS_LEFT, UI_PLOT_CANVAS_TOP,
                          UI_PLOT_CANVAS_WIDTH, UI_PLOT_CANVAS_HEIGHT);
}

static void UI_ReturnToMotor(void) {
    PlotterPath_Reset(&s_plotter_path);
    s_center_ready = 0u;
    s_motion_accepted = 0u;
    s_stop_requested = 0u;
    s_play_view_valid = 0u;
    s_demo_selected = 0u;
    s_ui_state = UI_PAGE_MOTOR;
    UI_DrawMotorView();
    UI_MotorSnapshotCapture();
}

static void UI_HandleMotorTouch(uint16_t x, uint16_t y) {
    MotorAxis_t target;
    uint8_t hit = 0u;

    if (UI_TouchInRect(x, y, UI_DRAW_BUTTON_X, UI_DRAW_BUTTON_Y,
                       UI_DRAW_BUTTON_W, UI_DRAW_BUTTON_H) != 0u) {
        MotorPlotState_t plot_state = Motor_PlotterGetState();

        if (Motor_JogIsRunning() != 0u ||
            plot_state == MOTOR_PLOT_PENDING ||
            plot_state == MOTOR_PLOT_RUNNING) {
            return;
        }
        s_center_ready = 0u;
        s_motion_accepted = 0u;
        s_stop_requested = 0u;
        s_play_view_valid = 0u;
        s_ui_state = UI_DRAW_CENTER_CONFIRM;
        UI_DrawCenterConfirmView();
        return;
    }

    if (y < UI_MOTOR_AXIS_BTN_Y ||
        y > (uint16_t)(UI_MOTOR_AXIS_BTN_Y + UI_MOTOR_AXIS_BTN_H)) {
        return;
    }

    if (x >= UI_MOTOR_AXIS_BTN_X1 &&
        x <= (uint16_t)(UI_MOTOR_AXIS_BTN_X1 + UI_MOTOR_AXIS_BTN_W)) {
        target = MOTOR_AXIS_X;
        hit = 1u;
    } else if (x >= UI_MOTOR_AXIS_BTN_X2 &&
               x <= (uint16_t)(UI_MOTOR_AXIS_BTN_X2 + UI_MOTOR_AXIS_BTN_W)) {
        target = MOTOR_AXIS_Y;
        hit = 1u;
    }

    if (hit == 0u || Motor_JogIsRunning() != 0u) {
        return;
    }

    Motor_SelectAxis(target);
    UI_DrawMotorState();
    UI_MotorSnapshotCapture();
}

static void UI_HandleTouch(void) {
    uint16_t x;
    uint16_t y;
    OS_ERR os_err;
    uint32_t now;

    if (GTP_GetTouchPoint(&x, &y) == 0u) {
        return;
    }

    now = OSTimeGet(&os_err);
    if ((uint32_t)(now - last_touch_time) < TOUCH_DEBOUNCE_MS) {
        return;
    }
    last_touch_time = now;

    UI_HandleMotorTouch(x, y);
}

static void UI_InvalidateStroke(const char *reason) {
    if (s_plotter_path.state == PLOTTER_PATH_CAPTURING) {
        s_plotter_path.state = PLOTTER_PATH_INVALID;
    }
    s_invalid_reason = (reason != NULL) ? reason : "INVALID STROKE";
    s_ui_state = UI_DRAW_INVALID;
    UI_DrawInvalidView(&s_plotter_path, s_invalid_reason);
}

static void UI_ConfirmCenter(void) {
    PlotterPath_Begin(&s_plotter_path, &s_plotter_canvas);
    if (s_plotter_path.state != PLOTTER_PATH_CAPTURING) {
        UI_InvalidateStroke("CANVAS ERROR");
        return;
    }

    s_center_ready = 1u;
    s_motion_accepted = 0u;
    s_stop_requested = 0u;
    s_demo_selected = 0u;
    s_ui_state = UI_DRAW_READY;
    UI_DrawReadyView();
}

static void UI_RedrawStroke(void) {
    if (s_center_ready == 0u || s_motion_accepted != 0u) {
        return;
    }

    PlotterPath_Begin(&s_plotter_path, &s_plotter_canvas);
    if (s_plotter_path.state != PLOTTER_PATH_CAPTURING) {
        UI_InvalidateStroke("CANVAS ERROR");
        return;
    }
    s_demo_selected = 0u;
    s_ui_state = UI_DRAW_READY;
    UI_DrawReadyView();
}

static void UI_AppendStrokePoint(uint16_t x, uint16_t y) {
    PlotterPathState_t path_state;

    /* Keep stored motion and the displayed stroke on the same board boundary. */
    if (x < UI_PLOT_CANVAS_LEFT) {
        x = UI_PLOT_CANVAS_LEFT;
    } else if (x >= UI_PLOT_CANVAS_LEFT + UI_PLOT_CANVAS_WIDTH) {
        x = UI_PLOT_CANVAS_LEFT + UI_PLOT_CANVAS_WIDTH - 1u;
    }
    if (y < UI_PLOT_CANVAS_TOP) {
        y = UI_PLOT_CANVAS_TOP;
    } else if (y >= UI_PLOT_CANVAS_TOP + UI_PLOT_CANVAS_HEIGHT) {
        y = UI_PLOT_CANVAS_TOP + UI_PLOT_CANVAS_HEIGHT - 1u;
    }

    path_state = PlotterPath_AppendTouch(&s_plotter_path, x, y);
    if (path_state != PLOTTER_PATH_CAPTURING) {
        if (path_state == PLOTTER_PATH_OUT_OF_BOUNDS) {
            UI_InvalidateStroke("OUTSIDE CANVAS");
        } else if (path_state == PLOTTER_PATH_OVERFLOW) {
            UI_InvalidateStroke("PATH BUFFER FULL");
        } else {
            UI_InvalidateStroke("INVALID STROKE");
        }
        return;
    }

    UI_DrawCanvasLine(s_last_touch_x, s_last_touch_y, x, y);
    s_last_touch_x = x;
    s_last_touch_y = y;
}

static void UI_StartPlayback(void) {
    if (s_ui_state != UI_DRAW_PREVIEW ||
        s_plotter_path.state != PLOTTER_PATH_READY ||
        s_center_ready == 0u || s_motion_accepted != 0u) {
        return;
    }

    if (Motor_PlotterRequestStart(&s_plotter_path) == 0u) {
        if (s_demo_selected != 0u) {
            UI_DrawDemoPreviewView(&s_plotter_path, "START REQUEST REJECTED");
        } else {
            UI_DrawPreviewView(&s_plotter_path, "START REQUEST REJECTED");
        }
        return;
    }

    s_stop_requested = 0u;
    s_play_view_valid = 1u;
    s_last_progress = 0u;
    s_last_plot_state = MOTOR_PLOT_PENDING;
    s_ui_state = UI_DRAW_PLAYING;
    UI_DrawPlayingView(&s_plotter_path, 0u, "WAITING FOR MOTOR");
}

static void UI_RequestPlotterStop(void) {
    if (s_ui_state != UI_DRAW_PLAYING || s_stop_requested != 0u) {
        return;
    }

    Motor_PlotterRequestStop();
    s_stop_requested = 1u;
    UI_DrawPlayingView(&s_plotter_path, s_last_progress, "STOP REQUESTED");
}

static void UI_HandleDrawEvent(const GTP_TouchEvent_t *event) {
    if (event == NULL || s_ui_state == UI_PAGE_MOTOR) {
        return;
    }

    if (s_ui_state == UI_DRAW_CENTER_CONFIRM) {
        if (event->type == GTP_EVENT_DOWN) {
            if (UI_TouchInRect(event->x, event->y,
                               UI_DRAW_CENTER_READY_X, UI_DRAW_CENTER_READY_Y,
                               UI_DRAW_CENTER_READY_W, UI_DRAW_CENTER_READY_H) != 0u) {
                /* This control DOWN confirms origin; it never seeds a stroke. */
                UI_ConfirmCenter();
            } else if (UI_TouchInRect(event->x, event->y,
                                      UI_DRAW_CENTER_BACK_X, UI_DRAW_CENTER_BACK_Y,
                                      UI_DRAW_CENTER_BACK_W, UI_DRAW_CENTER_BACK_H) != 0u) {
                UI_ReturnToMotor();
            }
        }
        return;
    }

    if (s_ui_state == UI_DRAW_DEMO_MENU) {
        uint8_t row;

        if (event->type != GTP_EVENT_DOWN) {
            return;
        }

        for (row = 0u; row < 3u; ++row) {
            uint16_t item_y = (uint16_t)(UI_DEMO_ITEM_Y + row * UI_DEMO_ITEM_PITCH);
            if (UI_TouchInRect(event->x, event->y, UI_DEMO_ITEM_X, item_y,
                               UI_DEMO_ITEM_W, UI_DEMO_ITEM_H) != 0u) {
                uint8_t demo = (uint8_t)(s_demo_page * 3u + row);
                if (demo < PlotterDemo_Count() && s_center_ready != 0u &&
                    s_motion_accepted == 0u &&
                    PlotterDemo_Load(&s_plotter_path, &s_plotter_canvas,
                                     (PlotterDemoId_t)demo) != 0u) {
                    s_selected_demo = (PlotterDemoId_t)demo;
                    s_demo_selected = 1u;
                    s_ui_state = UI_DRAW_PREVIEW;
                    UI_DrawDemoPreviewView(&s_plotter_path,
                                           PlotterDemo_Name(s_selected_demo));
                }
                return;
            }
        }

        if (UI_TouchInRect(event->x, event->y, UI_DEMO_BACK_X, UI_DEMO_NAV_Y,
                           UI_DRAW_ACTION_W, UI_DRAW_ACTION_H) != 0u) {
            if (s_demo_selected != 0u) {
                UI_RedrawStroke();
            } else {
                s_ui_state = UI_DRAW_READY;
                UI_DrawReadyView();
            }
        } else if (s_demo_page + 1u < (uint8_t)((PlotterDemo_Count() + 2u) / 3u) &&
                   UI_TouchInRect(event->x, event->y, UI_DEMO_NEXT_X, UI_DEMO_NAV_Y,
                                  UI_DRAW_ACTION_W, UI_DRAW_ACTION_H) != 0u) {
            ++s_demo_page;
            UI_DrawDemoMenuView(s_demo_page);
        } else if (s_demo_page != 0u &&
                   UI_TouchInRect(event->x, event->y, UI_DEMO_PREV_X, UI_DEMO_NAV_Y,
                                  UI_DRAW_ACTION_W, UI_DRAW_ACTION_H) != 0u) {
            --s_demo_page;
            UI_DrawDemoMenuView(s_demo_page);
        }
        return;
    }

    if (s_ui_state == UI_DRAW_READY) {
        if (event->type == GTP_EVENT_DOWN) {
            if (UI_TouchInCanvas(event->x, event->y) != 0u) {
                /* Machine starts at the confirmed center, not the first touch. */
                s_last_touch_x = UI_PLOT_CENTER_X;
                s_last_touch_y = UI_PLOT_CENTER_Y;
                s_ui_state = UI_DRAW_CAPTURING;
                UI_AppendStrokePoint(event->x, event->y);
            } else if (UI_TouchInRect(event->x, event->y,
                                      UI_DRAW_DEMO_BUTTON_X, UI_DRAW_ACTION_Y,
                                      UI_DRAW_ACTION_W, UI_DRAW_ACTION_H) != 0u) {
                if (s_center_ready != 0u && s_motion_accepted == 0u) {
                    s_demo_page = 0u;
                    s_ui_state = UI_DRAW_DEMO_MENU;
                    UI_DrawDemoMenuView(s_demo_page);
                }
            } else if (UI_TouchInRect(event->x, event->y,
                                      UI_DRAW_START_X, UI_DRAW_ACTION_Y,
                                      UI_DRAW_ACTION_W, UI_DRAW_ACTION_H) != 0u) {
                UI_ReturnToMotor();
            }
        } else if (event->type == GTP_EVENT_MULTI) {
            UI_InvalidateStroke("MULTI TOUCH");
        } else if (event->type == GTP_EVENT_ERROR) {
            UI_InvalidateStroke("TOUCH ERROR");
        }
        return;
    }

    if (s_ui_state == UI_DRAW_CAPTURING) {
        if (event->type == GTP_EVENT_MOVE) {
            UI_AppendStrokePoint(event->x, event->y);
        } else if (event->type == GTP_EVENT_UP) {
            if (PlotterPath_Finish(&s_plotter_path) == PLOTTER_PATH_READY) {
                s_ui_state = UI_DRAW_PREVIEW;
                s_invalid_reason = "INVALID STROKE";
                UI_DrawPreviewView(&s_plotter_path, "PATH READY");
            } else {
                UI_InvalidateStroke("STROKE TOO SHORT");
            }
        } else if (event->type == GTP_EVENT_MULTI) {
            UI_InvalidateStroke("MULTI TOUCH");
        } else if (event->type == GTP_EVENT_ERROR) {
            UI_InvalidateStroke("TOUCH ERROR");
        } else if (event->type == GTP_EVENT_DOWN) {
            UI_InvalidateStroke("UNEXPECTED TOUCH");
        }
        return;
    }

    if (event->type != GTP_EVENT_DOWN) {
        return;
    }

    if (s_ui_state == UI_DRAW_PREVIEW) {
        if (UI_TouchInRect(event->x, event->y,
                           UI_DRAW_START_X, UI_DRAW_ACTION_Y,
                           UI_DRAW_ACTION_W, UI_DRAW_ACTION_H) != 0u) {
            UI_StartPlayback();
        } else if (UI_TouchInRect(event->x, event->y,
                                  UI_DRAW_REDRAW_X, UI_DRAW_ACTION_Y,
                                  UI_DRAW_ACTION_W, UI_DRAW_ACTION_H) != 0u) {
            if (s_demo_selected != 0u) {
                s_ui_state = UI_DRAW_DEMO_MENU;
                UI_DrawDemoMenuView(s_demo_page);
            } else {
                UI_RedrawStroke();
            }
        } else if (UI_TouchInRect(event->x, event->y,
                                  UI_DRAW_CANCEL_X, UI_DRAW_ACTION_Y,
                                  UI_DRAW_ACTION_W, UI_DRAW_ACTION_H) != 0u) {
            if (s_demo_selected != 0u) {
                UI_RedrawStroke();
            } else {
                UI_ReturnToMotor();
            }
        }
    } else if (s_ui_state == UI_DRAW_INVALID) {
        if (UI_TouchInRect(event->x, event->y,
                           UI_DRAW_INVALID_REDRAW_X, UI_DRAW_ACTION_Y,
                           UI_DRAW_ACTION_W, UI_DRAW_ACTION_H) != 0u) {
            UI_RedrawStroke();
        } else if (UI_TouchInRect(event->x, event->y,
                                  UI_DRAW_INVALID_CANCEL_X, UI_DRAW_ACTION_Y,
                                  UI_DRAW_ACTION_W, UI_DRAW_ACTION_H) != 0u) {
            UI_ReturnToMotor();
        }
    } else if (s_ui_state == UI_DRAW_PLAYING) {
        if (UI_TouchInRect(event->x, event->y,
                           UI_DRAW_STOP_X, UI_DRAW_ACTION_Y,
                           UI_DRAW_STOP_W, UI_DRAW_ACTION_H) != 0u) {
            UI_RequestPlotterStop();
        }
    } else if (s_ui_state == UI_DRAW_RESULT) {
        if (UI_TouchInRect(event->x, event->y,
                           UI_DRAW_RESULT_BACK_X, UI_DRAW_RESULT_BACK_Y,
                           UI_DRAW_ACTION_W, UI_DRAW_ACTION_H) != 0u) {
            UI_ReturnToMotor();
        }
    }
}

static void UI_UpdatePlotterState(void) {
    MotorPlotState_t plot_state;
    uint16_t progress;

    if (s_ui_state != UI_DRAW_PLAYING) {
        return;
    }

    plot_state = Motor_PlotterGetState();
    if (plot_state == MOTOR_PLOT_PENDING) {
        if (s_play_view_valid == 0u || s_last_plot_state != MOTOR_PLOT_PENDING) {
            UI_DrawPlayingView(&s_plotter_path, 0u, "WAITING FOR MOTOR");
            s_play_view_valid = 1u;
            s_last_progress = 0u;
        }
        s_last_plot_state = MOTOR_PLOT_PENDING;
        return;
    }

    if (plot_state == MOTOR_PLOT_RUNNING) {
        /* RUNNING is published only after the motor task accepted engine start. */
        s_motion_accepted = 1u;
        s_center_ready = 0u;
        progress = Motor_PlotterGetProgress();
        if (s_last_plot_state != MOTOR_PLOT_RUNNING ||
            progress != s_last_progress) {
            UI_DrawPlayingView(&s_plotter_path, progress,
                               (s_stop_requested != 0u) ? "STOP REQUESTED" : "RUNNING");
        }
        s_play_view_valid = 1u;
        s_last_progress = progress;
        s_last_plot_state = MOTOR_PLOT_RUNNING;
        return;
    }

    if (plot_state == MOTOR_PLOT_REJECTED) {
        s_stop_requested = 0u;
        s_play_view_valid = 0u;
        if (s_motion_accepted == 0u && s_center_ready != 0u &&
            s_plotter_path.state == PLOTTER_PATH_READY) {
            /* Pre-motion rejection preserves both the frozen path and origin. */
            s_ui_state = UI_DRAW_PREVIEW;
            if (s_demo_selected != 0u) {
                UI_DrawDemoPreviewView(&s_plotter_path, "START REJECTED - RETRY");
            } else {
                UI_DrawPreviewView(&s_plotter_path, "START REJECTED - RETRY");
            }
        } else {
            s_center_ready = 0u;
            s_ui_state = UI_DRAW_RESULT;
            UI_DrawResultView(&s_plotter_path, "PLOTTER REJECTED");
        }
        return;
    }

    if (plot_state == MOTOR_PLOT_DONE) {
        s_center_ready = 0u;
        s_motion_accepted = 1u;
        s_stop_requested = 0u;
        s_play_view_valid = 0u;
        s_ui_state = UI_DRAW_RESULT;
        UI_DrawResultView(&s_plotter_path, "PLAYBACK COMPLETE");
        return;
    }

    if (plot_state == MOTOR_PLOT_ABORTED) {
        s_center_ready = 0u;
        s_stop_requested = 0u;
        s_play_view_valid = 0u;
        s_ui_state = UI_DRAW_RESULT;
        UI_DrawResultView(&s_plotter_path, "PLAYBACK ABORTED");
    }
}

void UI_Task(void *p_arg) {
    OS_ERR os_err;
    (void)p_arg;

    BOOT_STAGE(BOOT_STAGE_UI_TASK_ENTERED);

    NT35510_Init();
    BOOT_STAGE(BOOT_STAGE_LCD_INIT_DONE);

    NT35510_GramScan(LCD_SCAN_MODE);
    BOOT_STAGE(BOOT_STAGE_GRAMSCAN_DONE);

    I2C_Touch_Init();
    BOOT_STAGE(BOOT_STAGE_TOUCH_I2C_DONE);

    GTP_Init_Panel();
    BOOT_STAGE(BOOT_STAGE_TOUCH_PANEL_DONE);

    UI_DrawMotorView();
    UI_MotorSnapshotCapture();
    BOOT_STAGE(BOOT_STAGE_MOTOR_PAGE_DRAWN);

    while (1) {
        GTP_TouchEvent_t event = { GTP_EVENT_NONE, 0u, 0u, 0u };

        /* The ISR only sets the flag. Touch I2C and all LCD work stay here. */
        if (g_gtp_irq_flag != 0u) {
            g_gtp_irq_flag = 0u;
            GTP_TouchProcess(&event);
        }

        if (s_ui_state == UI_PAGE_MOTOR) {
            UI_HandleTouch();
            if (s_ui_state == UI_PAGE_MOTOR) {
                UI_MotorRefresh();
                OSTimeDly(50u, OS_OPT_TIME_DLY, &os_err);
            } else {
                OSTimeDly(10u, OS_OPT_TIME_DLY, &os_err);
            }
        } else {
            UI_HandleDrawEvent(&event);
            UI_UpdatePlotterState();
            OSTimeDly(10u, OS_OPT_TIME_DLY, &os_err);
        }
    }
}
