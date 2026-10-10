#include "plotter_path.h"

static int32_t round_div_symmetric(int32_t numerator, int32_t denominator)
{
    int32_t half_denominator = denominator / 2;

    if (numerator < 0) {
        return -(((-numerator) + half_denominator) / denominator);
    }
    return (numerator + half_denominator) / denominator;
}

static int8_t sign_i32(int32_t value)
{
    if (value > 0) {
        return 1;
    }
    if (value < 0) {
        return -1;
    }
    return 0;
}

static uint32_t abs_i32(int32_t value)
{
    return (uint32_t)(value < 0 ? -value : value);
}

static void clamp_touch_to_canvas(const PlotterCanvas_t *canvas,
                                 uint16_t *x,
                                 uint16_t *y)
{
    uint32_t right = (uint32_t)canvas->left + canvas->width - 1u;
    uint32_t bottom = (uint32_t)canvas->top + canvas->height - 1u;

    if (*x < canvas->left) {
        *x = canvas->left;
    } else if ((uint32_t)*x > right) {
        *x = (uint16_t)right;
    }

    if (*y < canvas->top) {
        *y = canvas->top;
    } else if ((uint32_t)*y > bottom) {
        *y = (uint16_t)bottom;
    }
}

void PlotterPath_Reset(PlotterPath_t *path)
{
    if (path == (PlotterPath_t *)0) {
        return;
    }

    path->count = 0u;
    path->canvas.left = 0u;
    path->canvas.top = 0u;
    path->canvas.width = 0u;
    path->canvas.height = 0u;
    path->state = PLOTTER_PATH_EMPTY;
}

void PlotterPath_Begin(PlotterPath_t *path, const PlotterCanvas_t *canvas)
{
    if (path == (PlotterPath_t *)0) {
        return;
    }

    PlotterPath_Reset(path);
    if (canvas == (const PlotterCanvas_t *)0) {
        path->state = PLOTTER_PATH_INVALID;
        return;
    }

    path->canvas = *canvas;
    if (canvas->width < 2u || canvas->height < 2u) {
        path->state = PLOTTER_PATH_INVALID;
        return;
    }

    path->points[0].x_steps = 0;
    path->points[0].y_steps = 0;
    path->count = 1u;
    path->state = PLOTTER_PATH_CAPTURING;
}

PlotterPathState_t PlotterPath_AppendTouch(PlotterPath_t *path,
                                           uint16_t x,
                                           uint16_t y)
{
    int32_t relative_x_twice;
    int32_t relative_y_twice;
    int32_t mapped_x;
    int32_t mapped_y;
    PlotterPoint_t *last_point;

    if (path == (PlotterPath_t *)0) {
        return PLOTTER_PATH_INVALID;
    }
    if (path->state != PLOTTER_PATH_CAPTURING) {
        return path->state;
    }
    clamp_touch_to_canvas(&path->canvas, &x, &y);

    relative_x_twice = 2 * ((int32_t)x - (int32_t)path->canvas.left) -
                       ((int32_t)path->canvas.width - 1);
    relative_y_twice = ((int32_t)path->canvas.height - 1) -
                       2 * ((int32_t)y - (int32_t)path->canvas.top);
    mapped_x = round_div_symmetric(relative_x_twice * PLOTTER_X_LIMIT_STEPS,
                                   (int32_t)path->canvas.width - 1);
    mapped_y = round_div_symmetric(relative_y_twice * PLOTTER_Y_LIMIT_STEPS,
                                   (int32_t)path->canvas.height - 1);

    last_point = &path->points[path->count - 1u];
    if (last_point->x_steps == (int16_t)mapped_x &&
        last_point->y_steps == (int16_t)mapped_y) {
        return path->state;
    }

    if (path->count >= PLOTTER_MAX_POINTS) {
        path->state = PLOTTER_PATH_OVERFLOW;
        return path->state;
    }

    path->points[path->count].x_steps = (int16_t)mapped_x;
    path->points[path->count].y_steps = (int16_t)mapped_y;
    ++path->count;
    return path->state;
}

PlotterPathState_t PlotterPath_Finish(PlotterPath_t *path)
{
    if (path == (PlotterPath_t *)0) {
        return PLOTTER_PATH_INVALID;
    }
    if (path->state == PLOTTER_PATH_OUT_OF_BOUNDS ||
        path->state == PLOTTER_PATH_OVERFLOW ||
        path->state == PLOTTER_PATH_INVALID) {
        return path->state;
    }
    if (path->state != PLOTTER_PATH_CAPTURING || path->count < 2u) {
        path->state = PLOTTER_PATH_INVALID;
        return path->state;
    }

    path->state = PLOTTER_PATH_READY;
    return path->state;
}

void PlotterDda_Begin(PlotterDda_t *dda, const PlotterPath_t *path)
{
    PlotterDda_t empty_dda = {0};

    if (dda == (PlotterDda_t *)0) {
        return;
    }

    *dda = empty_dda;
    if (path == (const PlotterPath_t *)0 ||
        path->state != PLOTTER_PATH_READY ||
        path->count < 2u || path->count > PLOTTER_MAX_POINTS) {
        return;
    }

    dda->path = path;
    dda->active = 1u;
}

uint8_t PlotterDda_Next(PlotterDda_t *dda,
                        uint8_t *axis_step_mask,
                        int8_t *dir_x,
                        int8_t *dir_y)
{
    if (axis_step_mask != (uint8_t *)0) {
        *axis_step_mask = 0u;
    }
    if (dir_x != (int8_t *)0) {
        *dir_x = 0;
    }
    if (dir_y != (int8_t *)0) {
        *dir_y = 0;
    }
    if (dda == (PlotterDda_t *)0 || axis_step_mask == (uint8_t *)0 ||
        dir_x == (int8_t *)0 || dir_y == (int8_t *)0 || dda->active == 0u ||
        dda->path == (const PlotterPath_t *)0) {
        return 0u;
    }

    while (dda->active != 0u) {
        const PlotterPath_t *path = dda->path;
        int32_t delta_x;
        int32_t delta_y;

        if (dda->segment_index >= (uint16_t)(path->count - 1u)) {
            dda->active = 0u;
            return 0u;
        }

        if (dda->segment_major_steps != 0u &&
            dda->segment_step_index >= dda->segment_major_steps) {
            ++dda->segment_index;
            dda->segment_step_index = 0u;
            dda->segment_major_steps = 0u;
            dda->abs_dx = 0u;
            dda->abs_dy = 0u;
            dda->accum_x = 0u;
            dda->accum_y = 0u;
            dda->dir_x = 0;
            dda->dir_y = 0;
            continue;
        }

        if (dda->segment_major_steps == 0u) {
            delta_x = (int32_t)path->points[dda->segment_index + 1u].x_steps -
                      (int32_t)path->points[dda->segment_index].x_steps;
            delta_y = (int32_t)path->points[dda->segment_index + 1u].y_steps -
                      (int32_t)path->points[dda->segment_index].y_steps;
            dda->abs_dx = abs_i32(delta_x);
            dda->abs_dy = abs_i32(delta_y);
            dda->dir_x = sign_i32(delta_x);
            dda->dir_y = sign_i32(delta_y);
            dda->segment_major_steps = dda->abs_dx > dda->abs_dy ?
                                       dda->abs_dx : dda->abs_dy;
            dda->segment_step_index = 0u;
            dda->accum_x = 0u;
            dda->accum_y = 0u;

            if (dda->segment_major_steps == 0u) {
                ++dda->segment_index;
                continue;
            }
        }

        dda->accum_x += dda->abs_dx;
        if (dda->accum_x >= dda->segment_major_steps) {
            dda->accum_x -= dda->segment_major_steps;
            *axis_step_mask |= PLOTTER_AXIS_X;
        }

        dda->accum_y += dda->abs_dy;
        if (dda->accum_y >= dda->segment_major_steps) {
            dda->accum_y -= dda->segment_major_steps;
            *axis_step_mask |= PLOTTER_AXIS_Y;
        }

        ++dda->segment_step_index;
        *dir_x = dda->dir_x;
        *dir_y = dda->dir_y;
        return 1u;
    }

    return 0u;
}
