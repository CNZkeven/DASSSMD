#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "plotter_path.h"

static const PlotterCanvas_t kCanvas = {24u, 220u, 433u, 285u};

static int8_t sign_of(int32_t value)
{
    if (value > 0) {
        return 1;
    }
    if (value < 0) {
        return -1;
    }
    return 0;
}

static void assert_touch_mapping(uint16_t x, uint16_t y,
                                 int16_t expected_x, int16_t expected_y)
{
    PlotterPath_t path;

    PlotterPath_Begin(&path, &kCanvas);
    assert(PlotterPath_AppendTouch(&path, 240u, 362u) == PLOTTER_PATH_CAPTURING);
    assert(PlotterPath_AppendTouch(&path, x, y) == PLOTTER_PATH_CAPTURING);
    assert(path.count == ((expected_x == 0 && expected_y == 0) ? 1u : 2u));
    assert(path.points[0].x_steps == 0);
    assert(path.points[0].y_steps == 0);
    if (path.count == 2u) {
        assert(path.points[1].x_steps == expected_x);
        assert(path.points[1].y_steps == expected_y);
    }
    assert(PlotterPath_Finish(&path) == PLOTTER_PATH_READY);
}

static void test_path_mapping_and_duplicate_compression(void)
{
    PlotterPath_t path;

    PlotterPath_Begin(&path, &kCanvas);
    assert(path.count == 1u);
    assert(path.points[0].x_steps == 0);
    assert(path.points[0].y_steps == 0);

    assert(PlotterPath_AppendTouch(&path, 240u, 362u) == PLOTTER_PATH_CAPTURING);
    assert(path.count == 1u);
    assert(PlotterPath_AppendTouch(&path, 456u, 362u) == PLOTTER_PATH_CAPTURING);
    assert(path.count == 2u);
    assert(path.points[1].x_steps == PLOTTER_X_LIMIT_STEPS);
    assert(path.points[1].y_steps == 0);
    assert(PlotterPath_AppendTouch(&path, 456u, 362u) == PLOTTER_PATH_CAPTURING);
    assert(path.count == 2u);

    assert(PlotterPath_AppendTouch(&path, 456u, 220u) == PLOTTER_PATH_CAPTURING);
    assert(path.count == 3u);
    assert(path.points[2].x_steps == PLOTTER_X_LIMIT_STEPS);
    assert(path.points[2].y_steps == PLOTTER_Y_LIMIT_STEPS);
    assert(PlotterPath_Finish(&path) == PLOTTER_PATH_READY);

    PlotterPath_Reset(&path);
    assert(path.count == 0u);
    assert(path.state == PLOTTER_PATH_EMPTY);
}

static void test_four_mapping_directions_and_inclusive_limits(void)
{
    assert_touch_mapping(456u, 362u, PLOTTER_X_LIMIT_STEPS, 0);
    assert_touch_mapping(24u, 362u, -PLOTTER_X_LIMIT_STEPS, 0);
    assert_touch_mapping(240u, 220u, 0, PLOTTER_Y_LIMIT_STEPS);
    assert_touch_mapping(240u, 504u, 0, -PLOTTER_Y_LIMIT_STEPS);
}

static void test_canvas_edges_map_to_signed_travel_limit_corners(void)
{
    PlotterPath_t path;

    PlotterPath_Begin(&path, &kCanvas);
    assert(PlotterPath_AppendTouch(&path, 24u, 220u) == PLOTTER_PATH_CAPTURING);
    assert(path.points[1].x_steps == -PLOTTER_X_LIMIT_STEPS);
    assert(path.points[1].y_steps == PLOTTER_Y_LIMIT_STEPS);
    assert(PlotterPath_AppendTouch(&path, 456u, 220u) == PLOTTER_PATH_CAPTURING);
    assert(path.points[2].x_steps == PLOTTER_X_LIMIT_STEPS);
    assert(path.points[2].y_steps == PLOTTER_Y_LIMIT_STEPS);
    assert(PlotterPath_AppendTouch(&path, 24u, 504u) == PLOTTER_PATH_CAPTURING);
    assert(path.points[3].x_steps == -PLOTTER_X_LIMIT_STEPS);
    assert(path.points[3].y_steps == -PLOTTER_Y_LIMIT_STEPS);
    assert(PlotterPath_AppendTouch(&path, 456u, 504u) == PLOTTER_PATH_CAPTURING);
    assert(path.points[4].x_steps == PLOTTER_X_LIMIT_STEPS);
    assert(path.points[4].y_steps == -PLOTTER_Y_LIMIT_STEPS);
    assert(PlotterPath_Finish(&path) == PLOTTER_PATH_READY);
}

static void test_touch_outside_canvas_is_clamped_to_nearest_edge(void)
{
    PlotterPath_t path;

    PlotterPath_Begin(&path, &kCanvas);
    assert(PlotterPath_AppendTouch(&path, 23u, 219u) == PLOTTER_PATH_CAPTURING);
    assert(path.count == 2u);
    assert(path.points[1].x_steps == -PLOTTER_X_LIMIT_STEPS);
    assert(path.points[1].y_steps == PLOTTER_Y_LIMIT_STEPS);
    assert(PlotterPath_AppendTouch(&path, 457u, 505u) == PLOTTER_PATH_CAPTURING);
    assert(path.count == 3u);
    assert(path.points[2].x_steps == PLOTTER_X_LIMIT_STEPS);
    assert(path.points[2].y_steps == -PLOTTER_Y_LIMIT_STEPS);
    assert(PlotterPath_Finish(&path) == PLOTTER_PATH_READY);
}

static void test_dda_traverses_from_center_to_first_absolute_touch(void)
{
    PlotterPath_t path;
    PlotterDda_t dda;
    uint32_t event_count = 0u;
    uint32_t x_steps = 0u;
    uint32_t y_steps = 0u;
    int32_t x = 0;
    int32_t y = 0;
    uint8_t mask;
    int8_t dir_x;
    int8_t dir_y;

    PlotterPath_Begin(&path, &kCanvas);
    assert(PlotterPath_AppendTouch(&path, 456u, 220u) == PLOTTER_PATH_CAPTURING);
    assert(PlotterPath_AppendTouch(&path, 24u, 504u) == PLOTTER_PATH_CAPTURING);
    assert(PlotterPath_Finish(&path) == PLOTTER_PATH_READY);
    assert(path.points[0].x_steps == 0 && path.points[0].y_steps == 0);
    assert(path.points[1].x_steps == PLOTTER_X_LIMIT_STEPS);
    assert(path.points[1].y_steps == PLOTTER_Y_LIMIT_STEPS);

    PlotterDda_Begin(&dda, &path);
    while (PlotterDda_Next(&dda, &mask, &dir_x, &dir_y) != 0u) {
        if ((mask & PLOTTER_AXIS_X) != 0u) {
            x += dir_x;
            ++x_steps;
        }
        if ((mask & PLOTTER_AXIS_Y) != 0u) {
            y += dir_y;
            ++y_steps;
        }
        ++event_count;
        if (event_count == (uint32_t)PLOTTER_X_LIMIT_STEPS) {
            assert(x == PLOTTER_X_LIMIT_STEPS);
            assert(y == PLOTTER_Y_LIMIT_STEPS);
        }
    }

    assert(x == -PLOTTER_X_LIMIT_STEPS);
    assert(y == -PLOTTER_Y_LIMIT_STEPS);
    assert(x_steps == 3u * (uint32_t)PLOTTER_X_LIMIT_STEPS);
    assert(y_steps == 3u * (uint32_t)PLOTTER_Y_LIMIT_STEPS);
}

static void test_finish_requires_at_least_two_points(void)
{
    PlotterPath_t path;

    PlotterPath_Begin(&path, &kCanvas);
    assert(PlotterPath_Finish(&path) == PLOTTER_PATH_INVALID);

    PlotterPath_Begin(&path, &kCanvas);
    assert(PlotterPath_AppendTouch(&path, 240u, 362u) == PLOTTER_PATH_CAPTURING);
    assert(PlotterPath_Finish(&path) == PLOTTER_PATH_INVALID);
    assert(PlotterPath_AppendTouch(&path, 456u, 362u) == PLOTTER_PATH_INVALID);
}

static void test_path_overflow_is_sticky(void)
{
    PlotterPath_t path;
    uint32_t index;

    PlotterPath_Begin(&path, &kCanvas);
    assert(PlotterPath_AppendTouch(&path, 240u, 362u) == PLOTTER_PATH_CAPTURING);
    for (index = 1u; index < PLOTTER_MAX_POINTS; ++index) {
        uint16_t x = ((index & 1u) != 0u) ? 24u : 240u;
        assert(PlotterPath_AppendTouch(&path, x, 362u) == PLOTTER_PATH_CAPTURING);
    }
    assert(path.count == PLOTTER_MAX_POINTS);
    assert(PlotterPath_AppendTouch(&path, 240u, 362u) == PLOTTER_PATH_OVERFLOW);
    assert(path.count == PLOTTER_MAX_POINTS);
    assert(PlotterPath_AppendTouch(&path, 24u, 362u) == PLOTTER_PATH_OVERFLOW);
    assert(PlotterPath_Finish(&path) == PLOTTER_PATH_OVERFLOW);
}

static void assert_dda_segment(int16_t x0, int16_t y0,
                               int16_t x1, int16_t y1,
                               uint32_t expected_events)
{
    PlotterPath_t path = {0};
    PlotterDda_t dda;
    int32_t x = x0;
    int32_t y = y0;
    int32_t dx = (int32_t)x1 - x0;
    int32_t dy = (int32_t)y1 - y0;
    int8_t expected_dir_x = sign_of(dx);
    int8_t expected_dir_y = sign_of(dy);
    uint32_t x_steps = 0u;
    uint32_t y_steps = 0u;
    uint32_t event_count = 0u;
    uint8_t mask;
    int8_t dir_x;
    int8_t dir_y;

    path.count = 2u;
    path.points[0].x_steps = x0;
    path.points[0].y_steps = y0;
    path.points[1].x_steps = x1;
    path.points[1].y_steps = y1;
    path.state = PLOTTER_PATH_READY;

    PlotterDda_Begin(&dda, &path);
    while (PlotterDda_Next(&dda, &mask, &dir_x, &dir_y) != 0u) {
        assert(mask != 0u);
        assert((mask & (uint8_t)~(PLOTTER_AXIS_X | PLOTTER_AXIS_Y)) == 0u);
        assert(dir_x == expected_dir_x);
        assert(dir_y == expected_dir_y);
        assert(++event_count <= expected_events);
        if ((mask & PLOTTER_AXIS_X) != 0u) {
            x += dir_x;
            ++x_steps;
        }
        if ((mask & PLOTTER_AXIS_Y) != 0u) {
            y += dir_y;
            ++y_steps;
        }
    }

    assert(event_count == expected_events);
    assert(x_steps == (uint32_t)(dx < 0 ? -dx : dx));
    assert(y_steps == (uint32_t)(dy < 0 ? -dy : dy));
    assert(x == x1);
    assert(y == y1);
    assert(mask == 0u);
    assert(dir_x == 0);
    assert(dir_y == 0);
}

static void test_dda_horizontal_vertical_diagonal_and_reverse_directions(void)
{
    assert_dda_segment(0, 0, 5, 0, 5u);
    assert_dda_segment(5, 0, 0, 0, 5u);
    assert_dda_segment(0, 0, 0, 4, 4u);
    assert_dda_segment(0, 4, 0, 0, 4u);
    assert_dda_segment(0, 0, 3, 3, 3u);

    assert_dda_segment(0, 0, 7, 3, 7u);
    assert_dda_segment(0, 0, -7, 3, 7u);
    assert_dda_segment(0, 0, -7, -3, 7u);
    assert_dda_segment(0, 0, 7, -3, 7u);
    assert_dda_segment(0, 0, 3, 7, 7u);
    assert_dda_segment(0, 0, -3, 7, 7u);
    assert_dda_segment(0, 0, -3, -7, 7u);
    assert_dda_segment(0, 0, 3, -7, 7u);
    assert_dda_segment(7, 3, 0, 0, 7u);
}

int main(void)
{
    test_path_mapping_and_duplicate_compression();
    test_four_mapping_directions_and_inclusive_limits();
    test_canvas_edges_map_to_signed_travel_limit_corners();
    test_touch_outside_canvas_is_clamped_to_nearest_edge();
    test_dda_traverses_from_center_to_first_absolute_touch();
    test_finish_requires_at_least_two_points();
    test_path_overflow_is_sticky();
    test_dda_horizontal_vertical_diagonal_and_reverse_directions();
    puts("plotter path host tests passed");
    return 0;
}
