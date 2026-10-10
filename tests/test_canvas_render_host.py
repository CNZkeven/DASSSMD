"""Check actual canvas rendering helpers and inverse pixel mapping on the host."""
import subprocess
import re

def function(source, name):
    match = re.search(r"(?:static\s+)?[A-Za-z_][A-Za-z0-9_ ]*\s+" + name + r"\s*\([^)]*\)\s*\{", source)
    assert match, name
    index, depth = match.end(), 1
    while depth:
        depth += (source[index] == "{") - (source[index] == "}")
        index += 1
    return source[match.start():index]
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_canvas_origin_marker_and_preview_mapping(tmp_path):
    source = (ROOT / 'User/ui/ui_draw.c').read_text()
    constants = '\n'.join(line for line in (ROOT / 'User/ui/ui_draw.h').read_text().splitlines()
                          if line.startswith('#define UI_') or line.startswith('#define COLOR_'))
    helpers = '\n'.join(function(source, name) for name in (
        'UI_DrawPlotCanvas', 'UI_RoundRatio', 'UI_PlotPointX', 'UI_PlotPointY', 'UI_DrawPlotterPath'))
    program = r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include "plotter_path.h"
static unsigned origin_h, origin_v, lines;
static uint16_t first_x, first_y, last_x, last_y;
static void UI_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    (void)color;
    assert(x + w <= 480 && y + h <= 800);
    if (x == 234 && y == 362 && w == 13 && h == 1) ++origin_h;
    if (x == 240 && y == 356 && w == 1 && h == 13) ++origin_v;
}
static void UI_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    (void)x; (void)y; (void)w; (void)h; (void)color;
}
static void UI_DrawCanvasLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    if (!lines) {first_x = x0; first_y = y0;}
    last_x = x1; last_y = y1; ++lines;
}
'''
    checks = r'''
int main(void) {
    PlotterPath_t path;
    PlotterCanvas_t canvas = {24, 220, 433, 285};
    unsigned x, y;
    UI_DrawPlotCanvas();
    assert(origin_h == 1 && origin_v == 1);
    for (y = 220; y <= 504; ++y) {
        for (x = 24; x <= 456; ++x) {
            PlotterPoint_t point;
            PlotterPath_Begin(&path, &canvas);
            assert(PlotterPath_AppendTouch(&path, x, y) == PLOTTER_PATH_CAPTURING);
            point = path.points[path.count - 1];
            assert(point.x_steps >= -3500 && point.x_steps <= 3500);
            assert(point.y_steps >= -2300 && point.y_steps <= 2300);
            assert(UI_PlotPointX(point.x_steps) == x);
            assert(UI_PlotPointY(point.y_steps) == y);
        }
    }
    PlotterPath_Begin(&path, &canvas);
    PlotterPath_AppendTouch(&path, 456, 220);
    PlotterPath_AppendTouch(&path, 24, 504);
    assert(PlotterPath_Finish(&path) == PLOTTER_PATH_READY);
    UI_DrawPlotterPath(&path);
    assert(lines == 2 && first_x == 240 && first_y == 362);
    assert(last_x == 24 && last_y == 504);
    return 0;
}
'''
    c_file = tmp_path / 'render.c'
    c_file.write_text('\n'.join((program, constants, helpers, checks)))
    executable = tmp_path / 'render'
    result = subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror',
                             '-I', str(ROOT / 'User/plotter'), str(c_file),
                             str(ROOT / 'User/plotter/plotter_path.c'), '-o', str(executable)],
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    result = subprocess.run([str(executable)], capture_output=True, text=True, cwd=tmp_path)
    assert result.returncode == 0, result.stderr
