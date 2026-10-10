#ifndef PLOTTER_DEMO_H
#define PLOTTER_DEMO_H

#include "plotter_path.h"

typedef enum {
    PLOTTER_DEMO_FLOWER = 0,
    PLOTTER_DEMO_CARRIER,
    PLOTTER_DEMO_FIGHTER,
    PLOTTER_DEMO_CHARACTER,
    PLOTTER_DEMO_DRAGON,
    PLOTTER_DEMO_HEART,
    PLOTTER_DEMO_SAIL_BADGE,
    PLOTTER_DEMO_TOGETHER,
    PLOTTER_DEMO_COUNT
} PlotterDemoId_t;

uint8_t PlotterDemo_Load(PlotterPath_t *path,
                         const PlotterCanvas_t *canvas,
                         PlotterDemoId_t demo);
const char *PlotterDemo_Name(PlotterDemoId_t demo);
uint8_t PlotterDemo_Count(void);

#endif
