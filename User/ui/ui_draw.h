#ifndef __UI_DRAW_H
#define __UI_DRAW_H

#include "stm32f4xx.h"
#include "plotter_path.h"

/*
*********************************************************************************************************
*                                       COLOR DEFINITIONS (RGB565)
*********************************************************************************************************
*/
#define COLOR_BG            0x18C5  // 深蓝色背景
#define COLOR_BTN_BG        0x1107  // 深灰色按钮背景
#define COLOR_BTN_BORDER    0x09AC  // 青色按钮边框
#define COLOR_TEXT          0xFFFF  // 白色文字
#define COLOR_SELECTED      0x4E5E  // 亮蓝色选中状态
#define COLOR_TITLE_BG      0x09AC  // 标题栏背景

#define COLOR_MOTOR_RUN     0x07E0  // 绿色：正在运行
#define COLOR_MOTOR_STOP    0x8410  // 灰色：停止

/*
*********************************************************************************************************
*                                            屏幕与布局
*
*  字体固定 16x32（LCD 默认字体），所以每行最多 480/16 = 30 个字符。
*********************************************************************************************************
*/
#define UI_SCREEN_WIDTH     480
#define UI_SCREEN_HEIGHT    800

#define UI_TITLE_Y          20
#define UI_TITLE_X          32
#define UI_TITLE_RIGHT_MARGIN 16

/* ---- 电机点动页 ---- */
#define UI_MOTOR_LABEL_Y        90

#define UI_MOTOR_AXIS_BTN_X1    40
#define UI_MOTOR_AXIS_BTN_X2    260
#define UI_MOTOR_AXIS_BTN_Y     140
#define UI_MOTOR_AXIS_BTN_W     180
#define UI_MOTOR_AXIS_BTN_H     120

#define UI_MOTOR_STATUS_X       40
#define UI_MOTOR_STATUS_VALUE_X 240
#define UI_MOTOR_STATUS_Y       310
#define UI_MOTOR_STATUS_PITCH   60

#define UI_MOTOR_HINT_Y         540
#define UI_MOTOR_HINT_PITCH     60

/* ---- One-stroke drawing layout ---- */
#define UI_DRAW_BUTTON_X            150u
#define UI_DRAW_BUTTON_Y            710u
#define UI_DRAW_BUTTON_W            180u
#define UI_DRAW_BUTTON_H            64u

#define UI_PLOT_CANVAS_LEFT          24u
#define UI_PLOT_CANVAS_TOP           220u
#define UI_PLOT_CANVAS_WIDTH         433u
#define UI_PLOT_CANVAS_HEIGHT        285u
#define UI_PLOT_CENTER_X             240u
#define UI_PLOT_CENTER_Y             362u

#define UI_DRAW_CENTER_READY_X        80u
#define UI_DRAW_CENTER_READY_Y       520u
#define UI_DRAW_CENTER_READY_W       320u
#define UI_DRAW_CENTER_READY_H        80u
#define UI_DRAW_CENTER_BACK_X         24u
#define UI_DRAW_CENTER_BACK_Y        700u
#define UI_DRAW_CENTER_BACK_W        130u
#define UI_DRAW_CENTER_BACK_H         64u

#define UI_DRAW_ACTION_Y             584u
#define UI_DRAW_START_X               20u
#define UI_DRAW_REDRAW_X             170u
#define UI_DRAW_CANCEL_X             320u
#define UI_DRAW_ACTION_W             140u
#define UI_DRAW_ACTION_H              64u
#define UI_DRAW_INVALID_REDRAW_X      90u
#define UI_DRAW_INVALID_CANCEL_X     250u
#define UI_DRAW_STOP_X               160u
#define UI_DRAW_STOP_W               160u
#define UI_DRAW_RESULT_BACK_X        175u
#define UI_DRAW_RESULT_BACK_Y        590u

#define UI_DRAW_DEMO_BUTTON_X        UI_DRAW_REDRAW_X
#define UI_DEMO_ITEM_X                80u
#define UI_DEMO_ITEM_Y               180u
#define UI_DEMO_ITEM_W               320u
#define UI_DEMO_ITEM_H                64u
#define UI_DEMO_ITEM_PITCH           100u
#define UI_DEMO_NAV_Y                UI_DRAW_ACTION_Y
#define UI_DEMO_BACK_X               UI_DRAW_START_X
#define UI_DEMO_PREV_X               UI_DRAW_REDRAW_X
#define UI_DEMO_NEXT_X               UI_DRAW_CANCEL_X

/*
*********************************************************************************************************
*                                            FUNCTION PROTOTYPES
*********************************************************************************************************
*/
void UI_DrawMotorView(void);
void UI_DrawMotorState(void);
void UI_DrawMotorStatus(void);
void UI_DrawCenterConfirmView(void);
void UI_DrawReadyView(void);
void UI_DrawDemoMenuView(uint8_t page);
void UI_DrawCanvasLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void UI_DrawPreviewView(const PlotterPath_t *path, const char *message);
void UI_DrawDemoPreviewView(const PlotterPath_t *path, const char *message);
void UI_DrawInvalidView(const PlotterPath_t *path, const char *reason);
void UI_DrawPlayingView(const PlotterPath_t *path,
                        uint16_t progress,
                        const char *status);
void UI_DrawResultView(const PlotterPath_t *path, const char *result);
void UI_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bgcolor);
void UI_DrawStringBounded(uint16_t x, uint16_t y, uint16_t max_width, const char *str, uint16_t color, uint16_t bgcolor);
void UI_DrawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
void UI_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);

#endif /* __UI_DRAW_H */
