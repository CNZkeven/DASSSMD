"""Run real timer scheduler/DDA against a CCR preload model, not real hardware."""
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    match = re.search(r'(?:static\s+)?(?:void|uint8_t|uint16_t|uint32_t|MotorPlotEngineState_t)\s+' + name + r'\s*\([^)]*\)\s*\{', source)
    assert match, name
    start, index, depth = match.start(), match.end(), 1
    while depth:
        depth += (source[index] == '{') - (source[index] == '}')
        index += 1
    return source[start:index]


def scheduler_program():
    source = (ROOT / 'User/motor/bsp_motor.c').read_text()
    header = (ROOT / 'User/motor/bsp_motor.h').read_text()
    engine_enum = re.search(r'typedef enum\s*\{\s*MOTOR_PLOT_ENGINE_IDLE.*?\}\s*MotorPlotEngineState_t;', header, re.S).group()
    config = '\n'.join(line for line in header.splitlines() if re.match(r'#define\s+(?:MOTOR_PULSE_CCR|MOTOR_DIR_SETUP_TICKS|PLOTTER_STEP_RATE_HZ)\s', line))
    state = source[source.index('static uint8_t           s_initialized'):source.index('/* DIR 引脚记录')]
    helpers = '\n'.join(function(source, name) for name in (
        'Motor_PlotterMeasurePath', 'Motor_PlotterWriteCompareMask',
        'Motor_PlotterCountCompletedMask', 'Motor_PlotterScheduleISR',
        'Motor_PlotterEngineStart', 'Motor_PlotterEngineRequestStop',
        'Motor_PlotterEngineGetState', 'Motor_PlotterEngineGetProgress', 'Motor_UpdateISR'))
    preamble = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "plotter_path.h"
typedef enum {MOTOR_AXIS_X, MOTOR_AXIS_Y, MOTOR_AXIS_COUNT} MotorAxis_t;
typedef enum {MOTOR_DIR_FORWARD, MOTOR_DIR_REVERSE} MotorDir_t;
#define TIM2 2
#define TIM_IT_Update 1
#define RESET 0
static uint32_t compares[2], physical_steps[2], clock_ticks, dir_tick[2], last_pulse[2];
static int32_t physical_position[2];
static MotorDir_t direction[2];
static uint8_t irq_pending = 1;
static void TIM_SetCompare2(unsigned timer, uint32_t value) { assert(timer == TIM2); compares[0] = value; }
static void TIM_SetCompare3(unsigned timer, uint32_t value) { assert(timer == TIM2); compares[1] = value; }
static uint8_t TIM_GetITStatus(unsigned timer, unsigned flag) { assert(timer == TIM2 && flag == TIM_IT_Update); return irq_pending; }
static void TIM_ClearITPendingBit(unsigned timer, unsigned flag) { assert(timer == TIM2 && flag == TIM_IT_Update); }
static void Motor_SetDirInternal(MotorAxis_t axis, MotorDir_t dir) {
    direction[axis] = dir;
    dir_tick[axis] = clock_ticks;
}
'''
    scenarios = r'''
/* At UEV, the hardware transfers previous CCR preload before ISR executes.
   Count pulse rising edges in this physical model independently of software. */
static void tick(void) {
    unsigned axis;
    uint8_t current_pulse[2];
    ++clock_ticks;
    for (axis = 0; axis < 2; ++axis) {
        current_pulse[axis] = compares[axis] != 0;
        if (compares[axis]) {
            assert(compares[axis] == MOTOR_PULSE_CCR);
            assert(clock_ticks - dir_tick[axis] >= MOTOR_DIR_SETUP_TICKS);
            if (last_pulse[axis]) assert(clock_ticks - last_pulse[axis] >= (1000u / PLOTTER_STEP_RATE_HZ));
            last_pulse[axis] = clock_ticks;
            ++physical_steps[axis];
            physical_position[axis] += direction[axis] == MOTOR_DIR_FORWARD ? 1 : -1;
        }
    }
    Motor_UpdateISR();
    /* Software counts only fully completed periods, not the current edge. */
    assert(s_steps[0] == physical_steps[0] - current_pulse[0]);
    assert(s_steps[1] == physical_steps[1] - current_pulse[1]);
}
static void reset_fixture(void) {
    s_initialized = 1;
    s_plot_state = MOTOR_PLOT_ENGINE_IDLE;
    s_plot_active_mask = s_plot_preload_mask = 0;
    memset((void *)s_pulsing, 0, sizeof(s_pulsing));
    memset((void *)s_steps, 0, sizeof(s_steps));
    memset(compares, 0, sizeof(compares));
    memset(physical_steps, 0, sizeof(physical_steps));
    memset(physical_position, 0, sizeof(physical_position));
    memset(dir_tick, 0, sizeof(dir_tick));
    memset(last_pulse, 0, sizeof(last_pulse));
    clock_ticks = 0;
}
static PlotterPath_t path;
static void drain(void) {
    unsigned i;
    for (i = 0; i < 3000 && (Motor_PlotterEngineGetState() == MOTOR_PLOT_ENGINE_RUNNING ||
                            Motor_PlotterEngineGetState() == MOTOR_PLOT_ENGINE_STOPPING); ++i) tick();
    assert(i < 3000);
    assert(compares[0] == 0 && compares[1] == 0);
    assert(s_steps[0] == physical_steps[0] && s_steps[1] == physical_steps[1]);
    assert(s_plot_active_mask == 0 && s_plot_preload_mask == 0);
}
int main(void) {
    unsigned phase, before;
    memset(&path, 0, sizeof(path));
    path.state = PLOTTER_PATH_READY;
    path.count = 3;
    path.points[1].x_steps = 7;
    path.points[1].y_steps = 3;
    path.points[2].x_steps = -2;
    path.points[2].y_steps = -4;
    reset_fixture();
    assert(Motor_PlotterEngineStart(&path));
    assert(!Motor_PlotterEngineStart(&path));
    drain();
    assert(Motor_PlotterEngineGetState() == MOTOR_PLOT_ENGINE_DONE);
    assert(Motor_PlotterEngineGetProgress() == 100);
    assert(physical_steps[0] == 16 && physical_steps[1] == 10);
    assert(physical_position[0] == -2 && physical_position[1] == -4);
    /* STOP throughout initial setup, queued CCR, active pulse and rate phases. */
    path.count = 2;
    path.points[1].x_steps = 100;
    path.points[1].y_steps = 40;
    for (phase = 0; phase < 30; ++phase) {
        reset_fixture();
        assert(Motor_PlotterEngineStart(&path));
        while (clock_ticks < phase) tick();
        Motor_PlotterEngineRequestStop();
        Motor_PlotterEngineRequestStop();
        drain();
        assert(Motor_PlotterEngineGetState() == MOTOR_PLOT_ENGINE_ABORTED);
        before = physical_steps[0] + physical_steps[1];
        tick(); tick(); tick(); tick();
        assert(before == physical_steps[0] + physical_steps[1]);
    }
    reset_fixture();
    s_pulsing[0] = 1;
    assert(!Motor_PlotterEngineStart(&path));
    s_pulsing[0] = 0;
    path.state = PLOTTER_PATH_INVALID;
    assert(!Motor_PlotterEngineStart(&path));
    assert(!Motor_PlotterEngineStart(NULL));
    return 0;
}
'''
    return '\n'.join((preamble, engine_enum, config, state, helpers, scenarios))


def run_model(tmp_path, program):
    c_file = tmp_path / 'scheduler.c'
    executable = tmp_path / 'scheduler'
    c_file.write_text(program)
    result = subprocess.run([
        'cc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-I', str(ROOT / 'User/plotter'),
        str(c_file), str(ROOT / 'User/plotter/plotter_path.c'), '-o', str(executable)
    ], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    return subprocess.run([str(executable)], capture_output=True, text=True, cwd=tmp_path)


def test_production_scheduler_pulses_directions_and_stop_phases(tmp_path):
    result = run_model(tmp_path, scheduler_program())
    assert result.returncode == 0, result.stderr


def test_plotter_step_rate_is_1000_steps_per_second():
    header = (ROOT / 'User/motor/bsp_motor.h').read_text()
    assert re.search(r'^#define\s+PLOTTER_STEP_RATE_HZ\s+1000u$', header, re.M)


def test_scheduler_model_detects_wrong_ccr_counting_slot(tmp_path):
    program = scheduler_program().replace(
        'Motor_PlotterCountCompletedMask(s_plot_active_mask);',
        'Motor_PlotterCountCompletedMask(s_plot_preload_mask);')
    result = run_model(tmp_path, program)
    assert result.returncode != 0, 'model did not catch wrong pulse accounting slot'
