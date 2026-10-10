# Centered Full-Range Canvas Implementation Plan

> **For agentic workers:** Use the TDD workflow for each behavior change. This plan is being executed inline in the current session; do not delegate or flash the board.

**Goal:** Make the drawing board cover the machine's full configured travel, centered on the manually confirmed machine origin, with preview and playback trajectories identical.

**Architecture:** `PlotterPath_Begin` seeds the logical origin `(0,0)` and touch samples map absolutely from the canvas rectangle to the signed machine travel. The UI clips active gestures at the board edge, draws the origin-to-first-touch segment, and preserves the existing DDA polyline playback.

**Tech Stack:** C99 firmware, STM32F407/uCOS-III, host C tests invoked by pytest, CMake/arm-none-eabi build.

**Spec:** `docs/superpowers/specs/2026-10-09-centered-full-range-canvas-design.md`

## Global Constraints

- The drawing canvas is `left=24`, `top=220`, `width=433`, `height=285`; its center is `(240,362)`.
- `PLOTTER_X_LIMIT_STEPS` remains `3500`; `PLOTTER_Y_LIMIT_STEPS` remains `2300`.
- The center is manually placed by the operator; do not add automatic motor motion or claim physical homing.
- The canvas edges map inclusively to `X=±3500` and `Y=±2300`; no accepted path point may exceed those limits.
- The path and preview include the physical center-to-first-touch connector; there is no pen-lift axis.
- Touch UP reports `(0,0)` in the current GT9xx driver; do not use its coordinates as a final path point.
- Do not flash or move the machine during software verification.

---

### Task 1: Absolute centered path mapping and origin-seeded DDA path

**Files:**
- Modify: `tests/plotter_path_host.c`
- Modify: `User/plotter/plotter_path.c`
- Modify: `User/plotter/plotter_path.h` only if existing path fields need cleanup

**Interfaces:**
- Consumes: `PlotterPath_Begin(PlotterPath_t *, const PlotterCanvas_t *)`, `PlotterPath_AppendTouch(PlotterPath_t *, uint16_t, uint16_t)`, `PlotterDda_Begin`, `PlotterDda_Next`.
- Produces: `PlotterPath_Begin` returns a CAPTURING path whose first point is `(0,0)`; AppendTouch stores an absolute, edge-clamped step coordinate.

- [x] **Step 1: Write failing centered-mapping tests.** Replace assumptions that the first touch is the origin. Add assertions that Begin seeds `(0,0)`, each center/edge/corner maps to the fixed machine coordinate, the first touch away from center creates an origin-to-touch segment, and outside pixels clamp to edge limits while the state remains CAPTURING.
- [x] **Step 2: Verify RED.** Run `pytest -q tests/test_plotter_path.py`; confirm failures are the old first-touch-relative mapping and out-of-canvas invalidation behavior.
- [x] **Step 3: Implement minimal mapping.** In Begin, seed point zero and set count to one after canvas validation. In AppendTouch, clamp pixel coordinates to the inclusive rectangle, calculate symmetric signed coordinates relative to the rectangle center, preserve duplicate compression and overflow behavior, and remove the first-touch-relative origin logic.
- [x] **Step 4: Verify GREEN.** Run `pytest -q tests/test_plotter_path.py`; confirm the compiled C host test passes with `-Wall -Wextra -Werror`.
- [x] **Step 5: Verify DDA physical start.** The C host test must build a path by Begin/Append/Finish, initialize DDA, and assert the first emitted direction moves from `(0,0)` toward its first non-center touch point and the event totals end at the final expected absolute point.

### Task 2: UI capture clipping and visible center-origin connector

**Files:**
- Modify: `User/ui/ui_task.c`
- Modify: `User/ui/ui_draw.c`
- Modify: `tests/test_static_contracts.py`

**Interfaces:**
- Consumes: the origin-seeded path and edge-clamped mapping from Task 1.
- Produces: a drawing UI that enters capture on a valid canvas DOWN regardless of path count, clips active MOVE points before both path storage and pixel rendering, draws center-to-first-touch, and leaves non-control outside DOWN untouched.

- [x] **Step 1: Write failing UI contract tests.** Assert the ready-to-capture path no longer requires `count == 1`, the first accepted touch draws from `UI_PLOT_CENTER_X/Y`, captured MOVE coordinates are clamped before both `PlotterPath_AppendTouch` and `UI_DrawCanvasLine`, and arbitrary outside-canvas DOWN does not call `UI_InvalidateStroke("OUTSIDE CANVAS")`.
- [x] **Step 2: Verify RED.** Run the newly added targeted tests in `tests/test_static_contracts.py`; verify they fail against the current UI flow.
- [x] **Step 3: Implement UI clipping/state transition.** Add a local coordinate clamp helper using the canvas constants. On DOWN inside the canvas, append the mapped absolute touch, enter CAPTURING on successful path state rather than a point-count condition, initialize last-touch pixels, and draw the connector from the canvas center. Clamp every MOVE before appending or rendering. Ignore blank DOWN outside the canvas while preserving the BACK control branch.
- [x] **Step 4: Add an origin marker.** Draw a small crosshair centered at `(UI_PLOT_CENTER_X, UI_PLOT_CENTER_Y)` from `UI_DrawPlotCanvas`; keep it inside the canvas border and use the same helper in ready, preview, playing, and result views.
- [x] **Step 5: Verify GREEN.** Run the targeted static UI tests and `pytest -q tests/test_plotter_path.py`; confirm current tests enforce the new fixed-origin behavior.

### Task 3: Full regression and build verification

**Files:**
- Review: `docs/superpowers/specs/2026-10-09-centered-full-range-canvas-design.md`
- Review: `docs/superpowers/plans/2026-10-09-centered-full-range-canvas.md`

- [x] **Step 1: Run all host tests.** Run `pytest -q`; require zero failures.
- [x] **Step 2: Build firmware.** Run `bash build.sh`; require exit status 0 and fresh `build/zhsx.elf`, `.hex`, and `.bin` outputs. Record any existing linker warnings accurately.
- [x] **Step 3: Review safety boundary.** Confirm no build/verification command flashes or starts motion, and document that physical travel, direction, and preview-to-machine alignment still require operator testing after the firmware is deliberately flashed.

## Execution record — 2026-10-09

- User approved the center-to-first-touch connector before completing UI integration.
- UI contracts were implemented as executable host tests in `tests/test_drawing_ui_host.py`, rather than new source-string assertions. The obsolete outside-DOWN rejection test was removed because outside-DOWN is now ignored and that behavior is tested in the real state machine harness.
- `tests/test_canvas_render_host.py` executes the production canvas/path rendering helpers and tests all 123,405 canvas pixels for bounded step coordinates and exact inverse mapping. It tests the origin marker and center-to-first-touch preview line. Removing the marker produced the expected failed assertion; restoring it passed.
- Path host tests verify seeded origin, four corner limits, edge clipping, compression, capacity, and DDA movement from center through both opposite corners.
- Final verification: `pytest -q` → 47 passed; `bash build.sh` → exit 0, refreshed ELF/HEX/BIN. Flash 61,628 B; RAM 27,664 B. Linker still reports the pre-existing RWX LOAD segment warning.
- No repository Git metadata is present; no commit or worktree was created. No flash, reset, or motor motion command was run.
- Firmware software requirements are implemented and verified on the host. Actual direction, physical edge clearance, lost-step behavior, and LCD/touch behavior need operator hardware testing after deliberate flashing; no physical homing or closed-loop guarantee is asserted.
