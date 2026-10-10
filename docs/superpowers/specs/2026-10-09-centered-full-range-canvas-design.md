# Centered Full-Range Drawing Canvas Design

## Goal

Make every point in the touchscreen drawing canvas correspond to a fixed point in the plotter's full software-defined travel range, with the canvas center equal to the manually established machine origin.

## Current Behavior and Problem

The drawing canvas is `433 x 285` pixels at `(24, 220)` on a `480 x 800` screen. Its center is `(240, 362)`. The configured machine travel is X `±3500` steps and Y `±2300` steps, and the canvas aspect ratio already closely matches those ranges.

Today the first touch is stored as `(0, 0)` and later points are measured relative to that touch. This makes the user's first finger location—not the canvas center—the machine origin. In addition, replay assumes the machine starts at the first stored path point. Changing only the pixel mapping would therefore make the on-screen path and physical path disagree whenever the first touch is not at the center.

## Considered Approaches

1. **Keep first-touch-relative coordinates.** Smallest change, but fails the fixed-center requirement and does not give the full canvas a stable machine coordinate system.
2. **Use absolute center-based coordinates and require the first touch at canvas center.** Keeps preview and replay aligned, but unnecessarily restricts where a one-stroke drawing can start.
3. **Use absolute center-based coordinates and include the center-to-first-touch segment.** Preserves free starting locations and makes the preview represent the actual trajectory. This is selected. The machine has no pen-lift axis, so the connector is physically drawn and must be visible in the preview.

## Coordinate and Canvas Rules

- Keep the current canvas rectangle and UI controls. The rectangle has the same aspect ratio, to rounding, as the configured X/Y travel.
- Inclusive canvas pixels map to inclusive software travel limits:
  - `(24, 362)` → `(-3500, 0)` steps.
  - `(240, 362)` → `(0, 0)` steps.
  - `(456, 362)` → `(+3500, 0)` steps.
  - `(240, 220)` → `(0, +2300)` steps.
  - `(240, 504)` → `(0, -2300)` steps.
  - All four corners map to the corresponding combinations of X/Y limits.
- Mapping is absolute and symmetric about the center. Use symmetric integer rounding so the center maps exactly to zero and edge pixels map exactly to the configured limits.
- A path begins with an explicit `(0, 0)` point. Every touch point is mapped absolutely, not relative to the first touch. Duplicate consecutive points remain compressed.
- The first valid DOWN within the canvas extends the path from the center to that point. Existing preview/replay path drawing and DDA then use the same explicit polyline.
- Touch DOWN outside the canvas does not start or invalidate a stroke (recognized UI controls keep their existing behavior). During an active stroke, MOVE points outside the canvas are clamped to the nearest canvas edge before path storage and display. Thus no accepted canvas gesture can request coordinates outside software travel limits.
- The GTP UP event carries `(0, 0)` rather than a final contact coordinate; do not append UP coordinates. Continue ending capture on UP as today.
- Draw a center/origin marker on the canvas so the physical starting point is apparent.

## Center Confirmation and Safety Boundary

The user must physically position the machine at the mechanical center before tapping `CENTER READY`. That confirmation establishes the software coordinate origin only; it does not move the motors or sense position. The design adds no homing, encoder, physical end-stop, pen-lift, or stall detection. The `±3500/±2300` limits remain software travel values supplied for this machine and must be validated on hardware before relying on the edges.

## Verification

Host tests must prove exact center and edge mappings, all four corner combinations, absolute first-touch mapping, explicit center-origin path construction, DDA traversal from center to the first touch, duplicate compression, clipping of out-of-canvas coordinates, and preservation of valid path completion. UI contract tests must prove canvas DOWN starts capture without relying on point count, the center-to-touch segment is rendered, outside MOVE events are clipped before rendering, and blank outside-canvas DOWN no longer invalidates a path. Run the complete host suite and firmware build. Hardware motion validation remains separate and must be performed cautiously by the operator.


## Built-in Vector Demonstrations

After center confirmation, `DEMO` opens a three-page selector for flower, aircraft carrier, fighter jet, an original laterally posed full-body character, dragon, mechanical heart, a simplified 1933 sailboat badge derived from the user-provided reference image, and a horizontal couple-profile portrait adapted from the user-provided line-art reference. Each demonstration is one continuous point sequence, explicitly including all connectors and returning to `(0,0)`; each is selected into preview only, and motion still requires a separate `START`. The vectors are generated offline by `tools/generate_plotter_demos.py`; one SVG and one flash-resident point table per subject must contain identical point sequences. No SVG parsing or curve math runs on the MCU. Every path must stay within X `±2800`, Y `±1840` steps, and `PLOTTER_MAX_POINTS`.
