"""Execute the production GT9xx parser with only its external I2C bus replaced.

Extract verbatim parser functions rather than copying their logic or adding test
hooks to vendor firmware. The fixture models fresh report bytes, bus failures,
and acknowledgements. This is not a substitute for board-level IRQ/I2C testing.
"""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def function_source(source, name):
    import re
    match = re.search(r"(?:static\s+)?(?:void|uint8_t)\s+" + name + r"\s*\([^)]*\)\s*\{", source)
    assert match, name
    start = match.start()
    depth = 1
    index = match.end()
    while depth:
        depth += (source[index] == "{") - (source[index] == "}")
        index += 1
    return source[start:index]


def parser_program():
    source = (ROOT / "User/touch/gt9xx.c").read_text(errors="replace")
    header = (ROOT / "User/touch/gt9xx.h").read_text(errors="replace")
    types = header[header.index("typedef enum {", header.index("uint8_t GTP_GetTouchPoint")):header.index("void GTP_TouchProcess(GTP_TouchEvent_t *event);")]
    state = source[source.index("static int16_t pre_x"):source.index("static void GTP_Touch_ResetState")]
    functions = "\n".join(function_source(source, name) for name in (
        "GTP_Touch_ResetState", "GTP_Touch_Down", "GTP_Touch_Up",
        "GTP_GetTouchPoint", "Goodix_TS_Work_Func", "GTP_TouchProcess"))
    bus = r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define GTP_MAX_TOUCH 5
#define GTP_READ_COOR_ADDR 0x814e
#define GTP_ADDR_LENGTH 2
#define GTP_ADDRESS 0xba
#define LCD_SCAN_MODE 3
#define LCD_X_LENGTH 480
#define LCD_Y_LENGTH 800
#define GTP_DEBUG_FUNC() ((void)0)
static uint8_t report_bytes[1 + 8 * GTP_MAX_TOUCH + 1];
static uint8_t fail_read, fail_write;
static unsigned acknowledgements;
static int32_t GTP_I2C_Read(uint8_t address, uint8_t *buf, int32_t len) {
    uint16_t reg = ((uint16_t)buf[0] << 8) | buf[1];
    (void)address;
    if (fail_read) return -1;
    assert(reg >= GTP_READ_COOR_ADDR);
    assert((unsigned)(reg - GTP_READ_COOR_ADDR + len - 2) <= sizeof(report_bytes));
    memcpy(buf + 2, report_bytes + reg - GTP_READ_COOR_ADDR, (size_t)len - 2);
    return 2;
}
static int32_t GTP_I2C_Write(uint8_t address, uint8_t *buf, int32_t len) {
    (void)address;
    assert(len == 3 && buf[0] == 0x81 && buf[1] == 0x4e && buf[2] == 0);
    ++acknowledgements;
    return fail_write ? -1 : 1;
}
'''
    fixtures = r'''
static void fresh(uint8_t count, uint8_t id, uint16_t x, uint16_t y) {
    unsigned i;
    memset(report_bytes, 0, sizeof(report_bytes));
    report_bytes[0] = 0x80 | count;
    for (i = 0; i < count && i < GTP_MAX_TOUCH; ++i) {
        unsigned offset = 1 + 8 * i;
        report_bytes[offset] = id + i;
        report_bytes[offset + 1] = x & 0xff;
        report_bytes[offset + 2] = x >> 8;
        report_bytes[offset + 3] = y & 0xff;
        report_bytes[offset + 4] = y >> 8;
    }
}
static GTP_TouchEvent_t process(uint8_t count, uint8_t id, uint16_t x, uint16_t y) {
    GTP_TouchEvent_t event;
    fresh(count, id, x, y);
    GTP_TouchProcess(&event);
    assert(event.touch_count == count);
    return event;
}
int main(void) {
    GTP_TouchEvent_t e;
    uint16_t x, y;
    unsigned before;
    GTP_Touch_ResetState();
    e = process(1, 0, 200, 300);
    assert(e.type == GTP_EVENT_DOWN && e.x == 200 && e.y == 300);
    assert(GTP_GetTouchPoint(&x, &y) == 1 && x == 200 && y == 300);
    e = process(1, 0, 210, 320);
    assert(e.type == GTP_EVENT_MOVE && e.x == 210 && e.y == 320);
    /* No fresh report is NONE, not a false UP. */
    report_bytes[0] = 0;
    before = acknowledgements;
    GTP_TouchProcess(&e);
    assert(e.type == GTP_EVENT_NONE && acknowledgements == before);
    assert(GTP_GetTouchPoint(&x, &y) == 1);
    e = process(0, 0, 0, 0);
    assert(e.type == GTP_EVENT_UP && GTP_GetTouchPoint(&x, &y) == 0);
    /* Staggered release must invalidate this gesture but permit a new one. */
    e = process(2, 0, 100, 200);
    assert(e.type == GTP_EVENT_MULTI && GTP_GetTouchPoint(&x, &y) == 0);
    e = process(1, 1, 120, 220);
    assert(e.type == GTP_EVENT_MULTI);
    e = process(0, 0, 0, 0);
    assert(e.type == GTP_EVENT_MULTI && g_touch_ambiguous == 0);
    e = process(1, 1, 240, 362);
    assert(e.type == GTP_EVENT_DOWN);
    e = process(1, 1, 250, 360);
    assert(e.type == GTP_EVENT_MOVE);
    /* Replacing ID without release cannot continue a single stroke. */
    e = process(1, 2, 250, 360);
    assert(e.type == GTP_EVENT_MULTI);
    e = process(0, 0, 0, 0);
    assert(g_touch_ambiguous == 0);
    e = process(1, 0, 100, 100);
    assert(e.type == GTP_EVENT_DOWN);
    fail_read = 1;
    GTP_TouchProcess(&e);
    assert(e.type == GTP_EVENT_ERROR && GTP_GetTouchPoint(&x, &y) == 0);
    fail_read = 0;
    e = process(1, 0, 110, 110);
    assert(e.type == GTP_EVENT_DOWN);
    fail_write = 1;
    GTP_TouchProcess(&e);
    assert(e.type == GTP_EVENT_ERROR && GTP_GetTouchPoint(&x, &y) == 0);
    fail_write = 0;
    e = process(1, 0, 110, 110);
    assert(e.type == GTP_EVENT_DOWN);
    return 0;
}
'''
    return bus + types + state + functions + fixtures


def run_program(tmp_path, program):
    c_file = tmp_path / "touch_events.c"
    executable = tmp_path / "touch_events"
    c_file.write_text(program)
    compile_result = subprocess.run(
        ["cc", "-std=c99", "-Wall", "-Wextra", "-Werror", str(c_file), "-o", str(executable)],
        capture_output=True, text=True)
    assert compile_result.returncode == 0, compile_result.stderr
    return subprocess.run([str(executable)], capture_output=True, text=True, cwd=tmp_path)


def test_production_touch_report_sequences(tmp_path):
    result = run_program(tmp_path, parser_program())
    assert result.returncode == 0, result.stderr


def test_touch_sequence_test_detects_missing_ambiguity_reset(tmp_path):
    # Mutation proof: remove ambiguity reset in the real state-reset helper.
    program = parser_program().replace("    g_touch_ambiguous = 0;", "    /* mutation: ambiguity remains latched */")
    result = run_program(tmp_path, program)
    assert result.returncode != 0, "sequence regression failed to detect sticky ambiguity"
