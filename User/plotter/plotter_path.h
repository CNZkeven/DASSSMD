#ifndef PLOTTER_PATH_H
#define PLOTTER_PATH_H

#include <stdint.h>

#define PLOTTER_MAX_POINTS       2048u
#define PLOTTER_X_LIMIT_STEPS    3500
#define PLOTTER_Y_LIMIT_STEPS    2300

#define PLOTTER_AXIS_X           0x01u
#define PLOTTER_AXIS_Y           0x02u

typedef struct {
    int16_t x_steps;
    int16_t y_steps;
} PlotterPoint_t;

typedef struct {
    uint16_t left;
    uint16_t top;
    uint16_t width;
    uint16_t height;
} PlotterCanvas_t;

typedef enum {
    PLOTTER_PATH_EMPTY = 0,
    PLOTTER_PATH_CAPTURING,
    PLOTTER_PATH_READY,
    PLOTTER_PATH_OUT_OF_BOUNDS,
    PLOTTER_PATH_OVERFLOW,
    PLOTTER_PATH_INVALID
} PlotterPathState_t;

typedef struct {
    PlotterPoint_t points[PLOTTER_MAX_POINTS];
    uint16_t count;
    PlotterCanvas_t canvas;
    PlotterPathState_t state;
} PlotterPath_t;

typedef struct {
    const PlotterPath_t *path;
    uint16_t segment_index;
    uint32_t segment_step_index;
    uint32_t segment_major_steps;
    uint32_t abs_dx;
    uint32_t abs_dy;
    uint32_t accum_x;
    uint32_t accum_y;
    int8_t dir_x;
    int8_t dir_y;
    uint8_t active;
} PlotterDda_t;

void PlotterPath_Begin(PlotterPath_t *path, const PlotterCanvas_t *canvas);
PlotterPathState_t PlotterPath_AppendTouch(PlotterPath_t *path,
                                           uint16_t x,
                                           uint16_t y);
PlotterPathState_t PlotterPath_Finish(PlotterPath_t *path);
void PlotterPath_Reset(PlotterPath_t *path);

void PlotterDda_Begin(PlotterDda_t *dda, const PlotterPath_t *path);
uint8_t PlotterDda_Next(PlotterDda_t *dda,
                        uint8_t *axis_step_mask,
                        int8_t *dir_x,
                        int8_t *dir_y);

#endif /* PLOTTER_PATH_H */
