"""Execute real drawing UI helpers plus path core, replacing LCD/motor edges.

Host tests cannot prove display timing, RTOS scheduling or physical motion. They
can verify command admission, immutable paths and touch-state transitions rather
than merely searching source strings.
"""
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def extract_function(source, name):
    match = re.search(r"static\s+(?:void|uint8_t)\s+" + name + r"\s*\([^)]*\)\s*\{", source)
    assert match, name
    start, index, depth = match.start(), match.end(), 1
    while depth:
        depth += (source[index] == "{") - (source[index] == "}")
        index += 1
    return source[start:index]


def ui_program():
    source = (ROOT / "User/ui/ui_task.c").read_text()
    ui_header = (ROOT / "User/ui/ui_task.h").read_text()
    state_enum = re.search(r"typedef enum\s*\{.*?\}\s*UI_State_t;", ui_header, re.S).group()
    touch_header = (ROOT / "User/touch/gt9xx.h").read_text(errors="replace")
    types = touch_header[touch_header.index("typedef enum {", touch_header.index("uint8_t GTP_GetTouchPoint")):touch_header.index("void GTP_TouchProcess(GTP_TouchEvent_t *event);")]
    motor_header = (ROOT / "User/task/motor_task.h").read_text()
    motor_enum = re.search(r"typedef enum\s*\{.*?\}\s*MotorPlotState_t;", motor_header, re.S).group()
    constants = "\n".join(line for line in (ROOT / "User/ui/ui_draw.h").read_text().splitlines() if line.startswith("#define UI_"))
    state = source[source.index("static UI_State_t s_ui_state"):source.index("typedef struct {")]
    helpers = "\n".join(extract_function(source, name) for name in (
        "UI_TouchInRect", "UI_TouchInCanvas", "UI_ReturnToMotor",
        "UI_InvalidateStroke", "UI_ConfirmCenter", "UI_RedrawStroke",
        "UI_AppendStrokePoint", "UI_StartPlayback", "UI_RequestPlotterStop",
        "UI_HandleDrawEvent", "UI_UpdatePlotterState"))
    includes = '#include <assert.h>\n#include <stdint.h>\n#include <stddef.h>\n#include <string.h>\n#include "plotter_path.h"\n#include "plotter_demo.h"\n'
    boundaries = r'''
static unsigned starts, stops, playing_draws, result_draws, menu_draws, demo_preview_draws;
static uint16_t line_x0, line_y0, line_x1, line_y1;
static uint8_t accept_start = 1;
static MotorPlotState_t motor_state = MOTOR_PLOT_IDLE;
static uint16_t motor_progress;
static const PlotterPath_t *submitted_path;
static uint8_t Motor_PlotterRequestStart(const PlotterPath_t *path) {
    ++starts;
    assert(path != NULL && path->state == PLOTTER_PATH_READY);
    if (!accept_start) return 0;
    submitted_path = path;
    motor_state = MOTOR_PLOT_PENDING;
    return 1;
}
static void Motor_PlotterRequestStop(void) { ++stops; }
static MotorPlotState_t Motor_PlotterGetState(void) { return motor_state; }
static uint16_t Motor_PlotterGetProgress(void) { return motor_progress; }
#define UI_DrawMotorView() ((void)0)
#define UI_MotorSnapshotCapture() ((void)0)
#define UI_DrawCenterConfirmView() ((void)0)
#define UI_DrawReadyView() ((void)0)
#define UI_DrawInvalidView(path, reason) ((void)(path), (void)(reason))
#define UI_DrawPreviewView(path, message) ((void)(path), (void)(message))
#define UI_DrawDemoMenuView(page) ((void)(page), ++menu_draws)
#define UI_DrawDemoPreviewView(path, message) ((void)(path), (void)(message), ++demo_preview_draws)
static void UI_DrawCanvasLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    line_x0 = x0; line_y0 = y0; line_x1 = x1; line_y1 = y1;
    assert(x0 >= 24 && x0 <= 456 && x1 >= 24 && x1 <= 456);
    assert(y0 >= 220 && y0 <= 504 && y1 >= 220 && y1 <= 504);
}
#define UI_DrawPlayingView(path,progress,status) ((void)(path),(void)(progress),(void)(status),++playing_draws)
#define UI_DrawResultView(path,result) ((void)(path),(void)(result),++result_draws)
'''
    scenarios = r'''
static void event(GTP_TouchEventType_t type, uint16_t x, uint16_t y) {
    GTP_TouchEvent_t e = {type, x, y, type == GTP_EVENT_UP ? 0 : 1};
    UI_HandleDrawEvent(&e);
}
static void confirm(void) {
    UI_ReturnToMotor();
    motor_state = MOTOR_PLOT_IDLE;
    accept_start = 1;
    s_ui_state = UI_DRAW_CENTER_CONFIRM;
    event(GTP_EVENT_DOWN, UI_DRAW_CENTER_READY_X + 1, UI_DRAW_CENTER_READY_Y + 1);
    assert(s_ui_state == UI_DRAW_READY && s_center_ready && s_plotter_path.count == 1);
    /* Sliding from CENTER READY cannot become a new stroke without a new DOWN. */
    event(GTP_EVENT_MOVE, 240, 362);
    assert(s_plotter_path.count == 1);
    event(GTP_EVENT_UP, 0, 0);
}
static void capture(void) {
    event(GTP_EVENT_DOWN, 240, 362);
    assert(s_ui_state == UI_DRAW_CAPTURING);
    assert(s_plotter_path.points[0].x_steps == 0 && s_plotter_path.points[0].y_steps == 0);
    event(GTP_EVENT_MOVE, 250, 362);
    event(GTP_EVENT_UP, 0, 0);
    assert(s_ui_state == UI_DRAW_PREVIEW && s_plotter_path.state == PLOTTER_PATH_READY);
    assert(s_plotter_path.count == 2 && s_plotter_path.points[1].x_steps == 162);
}
int main(void) {
    unsigned before, draws;
    PlotterPath_t frozen;
    /* Collecting and previewing never emits a START. */
    confirm();
    before = starts;
    capture();
    assert(starts == before);
    frozen = s_plotter_path;
    event(GTP_EVENT_DOWN, UI_DRAW_START_X + 1, UI_DRAW_ACTION_Y + 1);
    assert(starts == before + 1 && s_ui_state == UI_DRAW_PLAYING);
    assert(submitted_path == &s_plotter_path);
    UI_StartPlayback();
    assert(starts == before + 1); /* duplicate start while PENDING is ignored */
    event(GTP_EVENT_MOVE, 260, 370);
    event(GTP_EVENT_DOWN, UI_DRAW_REDRAW_X + 1, UI_DRAW_ACTION_Y + 1);
    event(GTP_EVENT_UP, 0, 0);
    assert(memcmp(&frozen, &s_plotter_path, sizeof(frozen)) == 0);
    assert(starts == before + 1);
    /* Reject before motion keeps the same confirmed center and path for retry. */
    motor_state = MOTOR_PLOT_REJECTED;
    UI_UpdatePlotterState();
    assert(s_ui_state == UI_DRAW_PREVIEW && s_center_ready && !s_motion_accepted);
    assert(memcmp(&frozen, &s_plotter_path, sizeof(frozen)) == 0);
    event(GTP_EVENT_DOWN, UI_DRAW_START_X + 1, UI_DRAW_ACTION_Y + 1);
    motor_state = MOTOR_PLOT_RUNNING;
    motor_progress = 10;
    UI_UpdatePlotterState();
    assert(!s_center_ready && s_motion_accepted);
    before = stops;
    event(GTP_EVENT_DOWN, UI_DRAW_STOP_X + 1, UI_DRAW_ACTION_Y + 1);
    draws = playing_draws;
    event(GTP_EVENT_DOWN, UI_DRAW_STOP_X + 1, UI_DRAW_ACTION_Y + 1);
    assert(stops == before + 1);
    UI_UpdatePlotterState();
    UI_UpdatePlotterState();
    assert(playing_draws == draws);
    assert(memcmp(&frozen, &s_plotter_path, sizeof(frozen)) == 0);
    motor_state = MOTOR_PLOT_ABORTED;
    UI_UpdatePlotterState();
    assert(s_ui_state == UI_DRAW_RESULT && !s_center_ready);
    assert(result_draws != 0);
    /* Blank/single point and invalid gestures cannot start. */
    confirm();
    before = starts;
    event(GTP_EVENT_DOWN, 240, 362);
    event(GTP_EVENT_UP, 0, 0);
    UI_StartPlayback();
    assert(s_ui_state == UI_DRAW_INVALID && starts == before);
    UI_RedrawStroke();
    event(GTP_EVENT_DOWN, 0, 0);
    assert(s_ui_state == UI_DRAW_READY);
    assert(s_plotter_path.count == 1);
    UI_RedrawStroke();
    event(GTP_EVENT_DOWN, 240, 362);
    event(GTP_EVENT_MULTI, 250, 362);
    assert(s_ui_state == UI_DRAW_INVALID);
    UI_StartPlayback();
    assert(starts == before);
    UI_RedrawStroke();
    event(GTP_EVENT_DOWN, 240, 362);
    event(GTP_EVENT_ERROR, 0, 0);
    assert(s_ui_state == UI_DRAW_INVALID && starts == before);
    /* A non-center DOWN includes the actual origin-to-touch connector. */
    confirm();
    before = starts;
    event(GTP_EVENT_DOWN, 456, 220);
    assert(s_ui_state == UI_DRAW_CAPTURING);
    assert(s_plotter_path.count == 2);
    assert(s_plotter_path.points[0].x_steps == 0 && s_plotter_path.points[0].y_steps == 0);
    assert(s_plotter_path.points[1].x_steps == 3500 && s_plotter_path.points[1].y_steps == 2300);
    assert(line_x0 == 240 && line_y0 == 362 && line_x1 == 456 && line_y1 == 220);
    /* Outside MOVE is clamped for both display and path, not invalidated. */
    event(GTP_EVENT_MOVE, 0, 799);
    assert(s_ui_state == UI_DRAW_CAPTURING && s_plotter_path.count == 3);
    assert(s_plotter_path.points[2].x_steps == -3500 && s_plotter_path.points[2].y_steps == -2300);
    assert(line_x0 == 456 && line_y0 == 220 && line_x1 == 24 && line_y1 == 504);
    assert(s_last_touch_x == 24 && s_last_touch_y == 504);
    event(GTP_EVENT_MOVE, UI_DRAW_START_X + 1, UI_DRAW_ACTION_Y + 1);
    assert(s_ui_state == UI_DRAW_CAPTURING && starts == before);
    event(GTP_EVENT_MOVE, 240, 362);
    assert(line_y0 == 504 && line_x1 == 240 && line_y1 == 362);
    assert(s_plotter_path.points[s_plotter_path.count - 1].x_steps == 0);
    assert(s_plotter_path.points[s_plotter_path.count - 1].y_steps == 0);
    event(GTP_EVENT_UP, 0, 0); /* UP has no contact coordinates: never append. */
    assert(s_ui_state == UI_DRAW_PREVIEW && starts == before);
    /* All eight choices load to preview only; explicit START owns motion. */
    confirm();
    before = starts;
    event(GTP_EVENT_DOWN, UI_DRAW_DEMO_BUTTON_X + 1, UI_DRAW_ACTION_Y + 1);
    assert(s_ui_state == UI_DRAW_DEMO_MENU && s_demo_page == 0 && menu_draws == 1);
    for (draws = 0; draws < PLOTTER_DEMO_COUNT; ++draws) {
        unsigned page = draws / 3;
        unsigned row = draws % 3;
        while (s_demo_page < page) {
            event(GTP_EVENT_DOWN, UI_DEMO_NEXT_X + 1, UI_DEMO_NAV_Y + 1);
            assert(s_ui_state == UI_DRAW_DEMO_MENU && s_demo_page <= page);
        }
        while (s_demo_page > page) {
            event(GTP_EVENT_DOWN, UI_DEMO_PREV_X + 1, UI_DEMO_NAV_Y + 1);
            assert(s_ui_state == UI_DRAW_DEMO_MENU && s_demo_page >= page);
        }
        event(GTP_EVENT_DOWN, UI_DEMO_ITEM_X + 1,
              UI_DEMO_ITEM_Y + row * UI_DEMO_ITEM_PITCH + 1);
        assert(s_ui_state == UI_DRAW_PREVIEW);
        assert(s_demo_selected && s_selected_demo == (PlotterDemoId_t)draws);
        assert(s_plotter_path.state == PLOTTER_PATH_READY && starts == before);
        assert(s_plotter_path.points[0].x_steps == 0 && s_plotter_path.points[0].y_steps == 0);
        assert(s_plotter_path.points[s_plotter_path.count-1].x_steps == 0);
        event(GTP_EVENT_DOWN, UI_DRAW_DEMO_BUTTON_X + 1, UI_DRAW_ACTION_Y + 1);
        assert(s_ui_state == UI_DRAW_DEMO_MENU && s_demo_page == page);
    }
    /* BACK from menu after a demo resets the canned path for hand drawing. */
    event(GTP_EVENT_DOWN, UI_DEMO_BACK_X + 1, UI_DEMO_NAV_Y + 1);
    assert(s_ui_state == UI_DRAW_READY && !s_demo_selected);
    assert(s_plotter_path.state == PLOTTER_PATH_CAPTURING && s_plotter_path.count == 1);
    event(GTP_EVENT_DOWN, UI_DRAW_DEMO_BUTTON_X + 1, UI_DRAW_ACTION_Y + 1);
    event(GTP_EVENT_DOWN, UI_DEMO_NEXT_X + 1, UI_DEMO_NAV_Y + 1);
    event(GTP_EVENT_DOWN, UI_DEMO_NEXT_X + 1, UI_DEMO_NAV_Y + 1);
    /* Page three exposes the couple-portrait example after the sail badge. */
    assert(s_ui_state == UI_DRAW_DEMO_MENU && s_demo_page == 2);
    event(GTP_EVENT_DOWN, UI_DEMO_ITEM_X + 1, UI_DEMO_ITEM_Y + UI_DEMO_ITEM_PITCH + 1);
    assert(s_selected_demo == PLOTTER_DEMO_TOGETHER && starts == before);
    event(GTP_EVENT_DOWN, UI_DRAW_START_X + 1, UI_DRAW_ACTION_Y + 1);
    assert(s_ui_state == UI_DRAW_PLAYING && starts == before + 1);
    /* Completed motion consumes origin even if UI never observed RUNNING. */
    confirm();
    capture();
    UI_StartPlayback();
    motor_state = MOTOR_PLOT_DONE;
    UI_UpdatePlotterState();
    assert(s_ui_state == UI_DRAW_RESULT && !s_center_ready && s_motion_accepted);
    return 0;
}
'''
    return '\n'.join((includes, state_enum, types, motor_enum, constants, boundaries, state, helpers, scenarios))


def run_ui(tmp_path, program):
    c_file = tmp_path / 'drawing_ui.c'
    c_file.write_text(program)
    executable = tmp_path / 'drawing_ui'
    result = subprocess.run([
        'cc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-I', str(ROOT / 'User/plotter'),
        str(c_file), str(ROOT / 'User/plotter/plotter_path.c'), str(ROOT / 'User/plotter/plotter_demo.c'), '-o', str(executable)
    ], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    return subprocess.run([str(executable)], capture_output=True, text=True, cwd=tmp_path)


def test_real_drawing_ui_capture_playback_lifecycle(tmp_path):
    result = run_ui(tmp_path, ui_program())
    assert result.returncode == 0, result.stderr


def test_ui_lifecycle_test_detects_missing_start_state_gate(tmp_path):
    program = ui_program().replace('s_ui_state != UI_DRAW_PREVIEW ||', '')
    result = run_ui(tmp_path, program)
    assert result.returncode != 0, 'lifecycle test did not catch invalid-state playback'
