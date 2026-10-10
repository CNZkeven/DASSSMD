#include "plotter_demo.h"

typedef struct {
    const PlotterPoint_t *points;
    uint16_t count;
    const char *name;
} PlotterDemoDef_t;

static const PlotterPoint_t s_flower_points[] = {
#include "plotter_flower_points.inc"
};
static const PlotterPoint_t s_carrier_points[] = {
#include "plotter_carrier_points.inc"
};
static const PlotterPoint_t s_fighter_points[] = {
#include "plotter_fighter_points.inc"
};
static const PlotterPoint_t s_character_points[] = {
#include "plotter_character_points.inc"
};
static const PlotterPoint_t s_dragon_points[] = {
#include "plotter_dragon_points.inc"
};
static const PlotterPoint_t s_heart_points[] = {
#include "plotter_heart_points.inc"
};
static const PlotterPoint_t s_sail_badge_points[] = {
#include "plotter_sail-badge_points.inc"
};
static const PlotterPoint_t s_together_points[] = {
#include "plotter_together_points.inc"
};

#define DEMO_DEF(points_, label_) \
    { (points_), (uint16_t)(sizeof(points_) / sizeof((points_)[0])), (label_) }

static const PlotterDemoDef_t s_demos[PLOTTER_DEMO_COUNT] = {
    DEMO_DEF(s_flower_points, "FLOWER"),
    DEMO_DEF(s_carrier_points, "CARRIER"),
    DEMO_DEF(s_fighter_points, "FIGHTER JET"),
    DEMO_DEF(s_character_points, "CHARACTER"),
    DEMO_DEF(s_dragon_points, "DRAGON"),
    DEMO_DEF(s_heart_points, "HEART GEAR"),
    DEMO_DEF(s_sail_badge_points, "1933 BADGE"),
    DEMO_DEF(s_together_points, "TOGETHER")
};

uint8_t PlotterDemo_Count(void)
{
    return (uint8_t)PLOTTER_DEMO_COUNT;
}

const char *PlotterDemo_Name(PlotterDemoId_t demo)
{
    if ((uint32_t)demo >= (uint32_t)PLOTTER_DEMO_COUNT) {
        return "?";
    }
    return s_demos[demo].name;
}

uint8_t PlotterDemo_Load(PlotterPath_t *path,
                         const PlotterCanvas_t *canvas,
                         PlotterDemoId_t demo)
{
    const PlotterDemoDef_t *definition;
    uint16_t i;

    if (path == (PlotterPath_t *)0 || canvas == (const PlotterCanvas_t *)0 ||
        (uint32_t)demo >= (uint32_t)PLOTTER_DEMO_COUNT ||
        canvas->width < 2u || canvas->height < 2u) {
        return 0u;
    }

    definition = &s_demos[demo];
    if (definition->count < 2u || definition->count > PLOTTER_MAX_POINTS ||
        definition->points[0].x_steps != 0 || definition->points[0].y_steps != 0 ||
        definition->points[definition->count - 1u].x_steps != 0 ||
        definition->points[definition->count - 1u].y_steps != 0) {
        return 0u;
    }

    for (i = 0u; i < definition->count; ++i) {
        if (definition->points[i].x_steps < -PLOTTER_X_LIMIT_STEPS ||
            definition->points[i].x_steps > PLOTTER_X_LIMIT_STEPS ||
            definition->points[i].y_steps < -PLOTTER_Y_LIMIT_STEPS ||
            definition->points[i].y_steps > PLOTTER_Y_LIMIT_STEPS) {
            return 0u;
        }
    }

    PlotterPath_Reset(path);
    path->canvas = *canvas;
    for (i = 0u; i < definition->count; ++i) {
        path->points[i] = definition->points[i];
    }
    path->count = definition->count;
    path->state = PLOTTER_PATH_READY;
    return 1u;
}
