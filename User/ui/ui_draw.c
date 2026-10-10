#include "ui_draw.h"
#include "bsp_nt35510_lcd.h"
#include "motor_task.h"
#include "plotter_demo.h"
#include "fonts.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/*
*********************************************************************************************************
*                                            绘制矩形填充
*********************************************************************************************************
*/
void UI_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
    LCD_SetBackColor(color);
    NT35510_Clear(x, y, width, height);
}

/*
*********************************************************************************************************
*                                            绘制矩形边框
*********************************************************************************************************
*/
void UI_DrawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
    UI_FillRect(x, y, width, 1, color);                     // 上边
    UI_FillRect(x, y + height - 1, width, 1, color);        // 下边
    UI_FillRect(x, y, 1, height, color);                    // 左边
    UI_FillRect(x + width - 1, y, 1, height, color);        // 右边
}

/*
*********************************************************************************************************
*                                            绘制字符串
*********************************************************************************************************
*/
void UI_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bgcolor) {
    if (x >= UI_SCREEN_WIDTH) {
        return;
    }
    UI_DrawStringBounded(x, y, UI_SCREEN_WIDTH - x, str, color, bgcolor);
}

/*
*********************************************************************************************************
*                                        绘制限定宽度字符串
*
*  LCD 用的是英文字库（NT35510_DispString_EN），所以屏上文字必须全是 ASCII。
*  超出 max_width 的部分会被截断，不会越界写到屏幕外。
*********************************************************************************************************
*/
void UI_DrawStringBounded(uint16_t x,
                          uint16_t y,
                          uint16_t max_width,
                          const char *str,
                          uint16_t color,
                          uint16_t bgcolor) {
    char buffer[64];
    uint16_t char_width;
    uint16_t max_chars;
    uint16_t i = 0;
    sFONT *font;

    if (str == NULL || x >= UI_SCREEN_WIDTH || y >= UI_SCREEN_HEIGHT || max_width == 0) {
        return;
    }

    if (x + max_width > UI_SCREEN_WIDTH) {
        max_width = UI_SCREEN_WIDTH - x;
    }

    font = LCD_GetFont();
    char_width = font->Width;
    max_chars = max_width / char_width;
    if (max_chars == 0) {
        return;
    }

    while (str[i] != '\0' && i < max_chars && i < (sizeof(buffer) - 1)) {
        buffer[i] = str[i];
        i++;
    }
    buffer[i] = '\0';

    LCD_SetTextColor(color);
    LCD_SetBackColor(bgcolor);
    NT35510_DispString_EN(x, y, buffer);
}

/*
*********************************************************************************************************
*                                            绘制标题栏
*********************************************************************************************************
*/
static void UI_DrawTitle(const char *title) {
    // 标题栏背景 + 分隔线
    UI_FillRect(0, 0, UI_SCREEN_WIDTH, 60, COLOR_TITLE_BG);
    UI_FillRect(0, 60, UI_SCREEN_WIDTH, 2, COLOR_BTN_BORDER);

    UI_DrawStringBounded(UI_TITLE_X,
                         UI_TITLE_Y,
                         UI_SCREEN_WIDTH - UI_TITLE_X - UI_TITLE_RIGHT_MARGIN,
                         title,
                         COLOR_TEXT,
                         COLOR_TITLE_BG);
}

static void UI_DrawPage(const char *title) {
    UI_FillRect(0, 0, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT, COLOR_BG);
    UI_DrawTitle(title);
}

static void UI_DrawButton(uint16_t x,
                          uint16_t y,
                          uint16_t width,
                          uint16_t height,
                          const char *label,
                          uint8_t enabled) {
    uint16_t text_width;
    uint16_t text_x;
    uint16_t text_y;
    uint16_t background = (enabled != 0u) ? COLOR_BTN_BG : COLOR_BG;

    UI_FillRect(x, y, width, height, background);
    UI_DrawRect(x, y, width, height,
                (enabled != 0u) ? COLOR_BTN_BORDER : COLOR_MOTOR_STOP);

    if (label == NULL) {
        return;
    }
    text_width = (uint16_t)(strlen(label) * 16u);
    text_x = (text_width < width) ? (uint16_t)(x + (width - text_width) / 2u) : x;
    text_y = (height > 32u) ? (uint16_t)(y + (height - 32u) / 2u) : y;
    UI_DrawStringBounded(text_x, text_y, width, label, COLOR_TEXT, background);
}

static void UI_DrawPlotCanvas(void) {
    UI_FillRect(UI_PLOT_CANVAS_LEFT, UI_PLOT_CANVAS_TOP,
                UI_PLOT_CANVAS_WIDTH, UI_PLOT_CANVAS_HEIGHT, COLOR_BTN_BG);
    UI_DrawRect(UI_PLOT_CANVAS_LEFT, UI_PLOT_CANVAS_TOP,
                UI_PLOT_CANVAS_WIDTH, UI_PLOT_CANVAS_HEIGHT, COLOR_BTN_BORDER);

    /* Mark the confirmed machine origin without leaving the canvas. */
    UI_FillRect(UI_PLOT_CENTER_X - 6u, UI_PLOT_CENTER_Y,
                13u, 1u, COLOR_SELECTED);
    UI_FillRect(UI_PLOT_CENTER_X, UI_PLOT_CENTER_Y - 6u,
                1u, 13u, COLOR_SELECTED);
}

static int32_t UI_RoundRatio(int32_t numerator, int32_t denominator) {
    int32_t half = denominator / 2;

    if (numerator < 0) {
        return -(((-numerator) + half) / denominator);
    }
    return (numerator + half) / denominator;
}

static uint16_t UI_PlotPointX(int16_t x_steps) {
    int32_t offset = UI_RoundRatio((int32_t)x_steps *
                                   (int32_t)(UI_PLOT_CANVAS_WIDTH - 1u),
                                   2 * PLOTTER_X_LIMIT_STEPS);

    return (uint16_t)((int32_t)UI_PLOT_CENTER_X + offset);
}

static uint16_t UI_PlotPointY(int16_t y_steps) {
    int32_t offset = UI_RoundRatio(-(int32_t)y_steps *
                                   (int32_t)(UI_PLOT_CANVAS_HEIGHT - 1u),
                                   2 * PLOTTER_Y_LIMIT_STEPS);

    return (uint16_t)((int32_t)UI_PLOT_CENTER_Y + offset);
}

static void UI_DrawPlotterPath(const PlotterPath_t *path) {
    uint16_t i;

    if (path == NULL || path->count > PLOTTER_MAX_POINTS) {
        return;
    }
    for (i = 1u; i < path->count; ++i) {
        UI_DrawCanvasLine(UI_PlotPointX(path->points[i - 1u].x_steps),
                          UI_PlotPointY(path->points[i - 1u].y_steps),
                          UI_PlotPointX(path->points[i].x_steps),
                          UI_PlotPointY(path->points[i].y_steps));
    }
}

/*
*********************************************************************************************************
*                                        绘制电机点动页（整页）
*********************************************************************************************************
*/
void UI_DrawMotorView(void) {
    // 清屏
    UI_FillRect(0, 0, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT, COLOR_BG);

    UI_DrawTitle("Motor Jog");

    UI_DrawString(UI_MOTOR_STATUS_X, UI_MOTOR_LABEL_Y, "Select Axis", COLOR_TEXT, COLOR_BG);

    // 操作提示
    UI_DrawString(UI_MOTOR_STATUS_X, UI_MOTOR_HINT_Y,
                  "KEY1 (hold) = Forward", COLOR_TEXT, COLOR_BG);
    UI_DrawString(UI_MOTOR_STATUS_X, UI_MOTOR_HINT_Y + UI_MOTOR_HINT_PITCH,
                  "KEY2 (hold) = Reverse", COLOR_TEXT, COLOR_BG);
    UI_DrawString(UI_MOTOR_STATUS_X, UI_MOTOR_HINT_Y + UI_MOTOR_HINT_PITCH * 2u,
                  "Release to stop", COLOR_SELECTED, COLOR_BG);

    UI_DrawButton(UI_DRAW_BUTTON_X, UI_DRAW_BUTTON_Y,
                  UI_DRAW_BUTTON_W, UI_DRAW_BUTTON_H, "DRAW", 1u);

    UI_DrawMotorState();
}

void UI_DrawCenterConfirmView(void) {
    UI_DrawPage("One Stroke Setup");
    UI_DrawString(40u, 150u, "Set tool at machine center",
                  COLOR_TEXT, COLOR_BG);
    UI_DrawString(40u, 210u, "Canvas center = tool (0,0)",
                  COLOR_TEXT, COLOR_BG);
    UI_DrawButton(UI_DRAW_CENTER_READY_X, UI_DRAW_CENTER_READY_Y,
                  UI_DRAW_CENTER_READY_W, UI_DRAW_CENTER_READY_H,
                  "CENTER READY", 1u);
    UI_DrawButton(UI_DRAW_CENTER_BACK_X, UI_DRAW_CENTER_BACK_Y,
                  UI_DRAW_CENTER_BACK_W, UI_DRAW_CENTER_BACK_H,
                  "BACK", 1u);
}

void UI_DrawReadyView(void) {
    UI_DrawPage("One Stroke - Ready");
    UI_DrawString(UI_PLOT_CANVAS_LEFT, 170u, "Draw one continuous stroke",
                  COLOR_TEXT, COLOR_BG);
    UI_DrawPlotCanvas();
    UI_DrawString(UI_PLOT_CANVAS_LEFT, 514u, "Center-to-touch is drawn",
                  COLOR_SELECTED, COLOR_BG);
    UI_DrawButton(UI_DRAW_START_X, UI_DRAW_ACTION_Y,
                  UI_DRAW_ACTION_W, UI_DRAW_ACTION_H, "BACK", 1u);
    UI_DrawButton(UI_DRAW_DEMO_BUTTON_X, UI_DRAW_ACTION_Y,
                  UI_DRAW_ACTION_W, UI_DRAW_ACTION_H, "DEMO", 1u);
}

void UI_DrawDemoMenuView(uint8_t page) {
    uint8_t row;
    uint8_t first_demo = (uint8_t)(page * 3u);

    if (page >= (uint8_t)((PLOTTER_DEMO_COUNT + 2u) / 3u)) {
        page = 0u;
        first_demo = 0u;
    }

    if (page == 0u) {
        UI_DrawPage("Examples 1/3");
    } else if (page == 1u) {
        UI_DrawPage("Examples 2/3");
    } else {
        UI_DrawPage("Examples 3/3");
    }
    UI_DrawString(UI_DEMO_ITEM_X, 118u, "Select a vector - preview before START",
                  COLOR_TEXT, COLOR_BG);
    for (row = 0u; row < 3u; ++row) {
        uint8_t demo = (uint8_t)(first_demo + row);
        if (demo < (uint8_t)PLOTTER_DEMO_COUNT) {
            UI_DrawButton(UI_DEMO_ITEM_X,
                          (uint16_t)(UI_DEMO_ITEM_Y + row * UI_DEMO_ITEM_PITCH),
                          UI_DEMO_ITEM_W, UI_DEMO_ITEM_H,
                          PlotterDemo_Name((PlotterDemoId_t)demo), 1u);
        }
    }

    UI_DrawButton(UI_DEMO_BACK_X, UI_DEMO_NAV_Y,
                  UI_DRAW_ACTION_W, UI_DRAW_ACTION_H, "BACK", 1u);
    UI_DrawButton(UI_DEMO_PREV_X, UI_DEMO_NAV_Y,
                  UI_DRAW_ACTION_W, UI_DRAW_ACTION_H,
                  (page == 0u) ? "PREV" : "PREV", (page != 0u));
    UI_DrawButton(UI_DEMO_NEXT_X, UI_DEMO_NAV_Y,
                  UI_DRAW_ACTION_W, UI_DRAW_ACTION_H,
                  (page >= 2u) ? "NEXT" : "NEXT",
                  (uint8_t)(page + 1u < (PLOTTER_DEMO_COUNT + 2u) / 3u));
}

void UI_DrawCanvasLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    int32_t x = x0;
    int32_t y = y0;
    int32_t end_x = x1;
    int32_t end_y = y1;
    int32_t dx = (end_x >= x) ? (end_x - x) : (x - end_x);
    int32_t sx = (x < end_x) ? 1 : -1;
    int32_t dy = (end_y >= y) ? (end_y - y) : (y - end_y);
    int32_t sy = (y < end_y) ? 1 : -1;
    int32_t error = dx - dy;
    int32_t left = UI_PLOT_CANVAS_LEFT;
    int32_t top = UI_PLOT_CANVAS_TOP;
    int32_t right = left + UI_PLOT_CANVAS_WIDTH;
    int32_t bottom = top + UI_PLOT_CANVAS_HEIGHT;

    for (;;) {
        if (x >= left && x < right && y >= top && y < bottom &&
            x >= 0 && x < UI_SCREEN_WIDTH && y >= 0 && y < UI_SCREEN_HEIGHT) {
            UI_FillRect((uint16_t)x, (uint16_t)y, 1u, 1u, COLOR_SELECTED);
        }
        if (x == end_x && y == end_y) {
            break;
        }
        {
            int32_t twice_error = 2 * error;
            if (twice_error > -dy) {
                error -= dy;
                x += sx;
            }
            if (twice_error < dx) {
                error += dx;
                y += sy;
            }
        }
    }
}

void UI_DrawPreviewView(const PlotterPath_t *path, const char *message) {
    char limits[48];

    UI_DrawPage("One Stroke - Preview");
    UI_DrawPlotCanvas();
    UI_DrawPlotterPath(path);
    UI_DrawString(UI_PLOT_CANVAS_LEFT, 514u,
                  (message != NULL) ? message : "PREVIEW READY",
                  COLOR_SELECTED, COLOR_BG);
    snprintf(limits, sizeof(limits), "LIMIT X +/- %d Y +/- %d",
             PLOTTER_X_LIMIT_STEPS, PLOTTER_Y_LIMIT_STEPS);
    UI_DrawString(UI_PLOT_CANVAS_LEFT, 546u, limits, COLOR_TEXT, COLOR_BG);
    UI_DrawButton(UI_DRAW_START_X, UI_DRAW_ACTION_Y,
                  UI_DRAW_ACTION_W, UI_DRAW_ACTION_H, "START",
                  (path != NULL && path->state == PLOTTER_PATH_READY));
    UI_DrawButton(UI_DRAW_REDRAW_X, UI_DRAW_ACTION_Y,
                  UI_DRAW_ACTION_W, UI_DRAW_ACTION_H, "REDRAW", 1u);
    UI_DrawButton(UI_DRAW_CANCEL_X, UI_DRAW_ACTION_Y,
                  UI_DRAW_ACTION_W, UI_DRAW_ACTION_H, "CANCEL", 1u);
}

void UI_DrawDemoPreviewView(const PlotterPath_t *path, const char *message) {
    char limits[48];

    UI_DrawPage("Demo - Preview");
    UI_DrawPlotCanvas();
    UI_DrawPlotterPath(path);
    UI_DrawString(UI_PLOT_CANVAS_LEFT, 514u,
                  (message != NULL) ? message : "DEMO PREVIEW",
                  COLOR_SELECTED, COLOR_BG);
    snprintf(limits, sizeof(limits), "LIMIT X +/- %d Y +/- %d",
             PLOTTER_X_LIMIT_STEPS, PLOTTER_Y_LIMIT_STEPS);
    UI_DrawString(UI_PLOT_CANVAS_LEFT, 546u, limits, COLOR_TEXT, COLOR_BG);
    UI_DrawButton(UI_DRAW_START_X, UI_DRAW_ACTION_Y,
                  UI_DRAW_ACTION_W, UI_DRAW_ACTION_H, "START",
                  (path != NULL && path->state == PLOTTER_PATH_READY));
    UI_DrawButton(UI_DRAW_REDRAW_X, UI_DRAW_ACTION_Y,
                  UI_DRAW_ACTION_W, UI_DRAW_ACTION_H, "EXAMPLES", 1u);
    UI_DrawButton(UI_DRAW_CANCEL_X, UI_DRAW_ACTION_Y,
                  UI_DRAW_ACTION_W, UI_DRAW_ACTION_H, "READY", 1u);
}

void UI_DrawInvalidView(const PlotterPath_t *path, const char *reason) {
    (void)path;
    UI_DrawPage("One Stroke - Invalid");
    UI_DrawString(UI_PLOT_CANVAS_LEFT, 514u,
                  (reason != NULL) ? reason : "INVALID STROKE",
                  COLOR_MOTOR_RUN, COLOR_BG);
    UI_DrawString(UI_PLOT_CANVAS_LEFT, 546u, "REDRAW TO TRY AGAIN",
                  COLOR_TEXT, COLOR_BG);
    UI_DrawButton(UI_DRAW_INVALID_REDRAW_X, UI_DRAW_ACTION_Y,
                  UI_DRAW_ACTION_W, UI_DRAW_ACTION_H, "REDRAW", 1u);
    UI_DrawButton(UI_DRAW_INVALID_CANCEL_X, UI_DRAW_ACTION_Y,
                  UI_DRAW_ACTION_W, UI_DRAW_ACTION_H, "CANCEL", 1u);
}

void UI_DrawPlayingView(const PlotterPath_t *path,
                        uint16_t progress,
                        const char *status) {
    char line[32];

    UI_DrawPage("One Stroke - Playing");
    UI_DrawPlotCanvas();
    UI_DrawPlotterPath(path);
    UI_DrawString(UI_PLOT_CANVAS_LEFT, 514u,
                  (status != NULL) ? status : "RUNNING",
                  COLOR_SELECTED, COLOR_BG);
    snprintf(line, sizeof(line), "PROGRESS: %u%%", (unsigned int)progress);
    UI_DrawString(UI_PLOT_CANVAS_LEFT, 546u, line, COLOR_TEXT, COLOR_BG);
    UI_DrawButton(UI_DRAW_STOP_X, UI_DRAW_ACTION_Y,
                  UI_DRAW_STOP_W, UI_DRAW_ACTION_H, "STOP", 1u);
}

void UI_DrawResultView(const PlotterPath_t *path, const char *result) {
    UI_DrawPage("One Stroke - Result");
    UI_DrawPlotCanvas();
    UI_DrawPlotterPath(path);
    UI_DrawString(UI_PLOT_CANVAS_LEFT, 514u,
                  (result != NULL) ? result : "PLAYBACK COMPLETE",
                  COLOR_SELECTED, COLOR_BG);
    UI_DrawString(UI_PLOT_CANVAS_LEFT, 546u, "CENTER CONFIRMATION CLEARED",
                  COLOR_TEXT, COLOR_BG);
    UI_DrawButton(UI_DRAW_RESULT_BACK_X, UI_DRAW_RESULT_BACK_Y,
                  UI_DRAW_ACTION_W, UI_DRAW_ACTION_H, "BACK", 1u);
}

/*
*********************************************************************************************************
*                          绘制轴选择按钮 + 状态区（切轴触摸后调用）
*********************************************************************************************************
*/
void UI_DrawMotorState(void) {
    MotorAxis_t axis = Motor_GetSelectedAxis();
    uint16_t bg_x = (axis == MOTOR_AXIS_X) ? COLOR_SELECTED : COLOR_BTN_BG;
    uint16_t bg_y = (axis == MOTOR_AXIS_Y) ? COLOR_SELECTED : COLOR_BTN_BG;
    uint16_t bd_x = (axis == MOTOR_AXIS_X) ? COLOR_TEXT : COLOR_BTN_BORDER;
    uint16_t bd_y = (axis == MOTOR_AXIS_Y) ? COLOR_TEXT : COLOR_BTN_BORDER;

    // X 轴按钮
    UI_FillRect(UI_MOTOR_AXIS_BTN_X1, UI_MOTOR_AXIS_BTN_Y,
                UI_MOTOR_AXIS_BTN_W, UI_MOTOR_AXIS_BTN_H, bg_x);
    UI_DrawRect(UI_MOTOR_AXIS_BTN_X1, UI_MOTOR_AXIS_BTN_Y,
                UI_MOTOR_AXIS_BTN_W, UI_MOTOR_AXIS_BTN_H, bd_x);
    UI_DrawString(UI_MOTOR_AXIS_BTN_X1 + 42, UI_MOTOR_AXIS_BTN_Y + 44,
                  "X AXIS", COLOR_TEXT, bg_x);

    // Y 轴按钮
    UI_FillRect(UI_MOTOR_AXIS_BTN_X2, UI_MOTOR_AXIS_BTN_Y,
                UI_MOTOR_AXIS_BTN_W, UI_MOTOR_AXIS_BTN_H, bg_y);
    UI_DrawRect(UI_MOTOR_AXIS_BTN_X2, UI_MOTOR_AXIS_BTN_Y,
                UI_MOTOR_AXIS_BTN_W, UI_MOTOR_AXIS_BTN_H, bd_y);
    UI_DrawString(UI_MOTOR_AXIS_BTN_X2 + 42, UI_MOTOR_AXIS_BTN_Y + 44,
                  "Y AXIS", COLOR_TEXT, bg_y);

    UI_DrawMotorStatus();
}

/*
*********************************************************************************************************
*                        只重绘状态区（周期性刷新用，触摸按钮不动）
*********************************************************************************************************
*/
void UI_DrawMotorStatus(void) {
    char line[32];
    MotorAxis_t axis = Motor_GetSelectedAxis();
    uint8_t running = Motor_JogIsRunning();

    // 整块擦掉再画：步数位数变化时不会留下上一帧的残影
    UI_FillRect(UI_MOTOR_STATUS_X, UI_MOTOR_STATUS_Y,
                UI_SCREEN_WIDTH - UI_MOTOR_STATUS_X * 2u,
                UI_MOTOR_STATUS_PITCH * 3u,
                COLOR_BG);

    UI_DrawString(UI_MOTOR_STATUS_X, UI_MOTOR_STATUS_Y,
                  "Status :", COLOR_TEXT, COLOR_BG);
    UI_DrawString(UI_MOTOR_STATUS_VALUE_X, UI_MOTOR_STATUS_Y,
                  (running != 0u) ? "RUNNING" : "STOPPED",
                  (running != 0u) ? COLOR_MOTOR_RUN : COLOR_MOTOR_STOP,
                  COLOR_BG);

    UI_DrawString(UI_MOTOR_STATUS_X, UI_MOTOR_STATUS_Y + UI_MOTOR_STATUS_PITCH,
                  "Dir    :", COLOR_TEXT, COLOR_BG);
    UI_DrawString(UI_MOTOR_STATUS_VALUE_X, UI_MOTOR_STATUS_Y + UI_MOTOR_STATUS_PITCH,
                  (running != 0u) ? ((Motor_JogGetDir() == MOTOR_DIR_FORWARD) ? "FORWARD"
                                                                            : "REVERSE")
                                  : "--",
                  COLOR_TEXT, COLOR_BG);

    UI_DrawString(UI_MOTOR_STATUS_X, UI_MOTOR_STATUS_Y + UI_MOTOR_STATUS_PITCH * 2u,
                  "Steps  :", COLOR_TEXT, COLOR_BG);
    snprintf(line, sizeof(line), "%lu", (unsigned long)Motor_GetSteps(axis));
    UI_DrawString(UI_MOTOR_STATUS_VALUE_X, UI_MOTOR_STATUS_Y + UI_MOTOR_STATUS_PITCH * 2u,
                  line, COLOR_SELECTED, COLOR_BG);
}
