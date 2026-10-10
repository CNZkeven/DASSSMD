# STM32F407 双轴一笔画作画实施计划

> **For agentic workers:** 使用 `superpowers:executing-plans` 在本会话逐任务执行；涉及每个生产逻辑改动时先使用 `superpowers:test-driven-development`。逐步骤按复选框跟踪。

**目标：** 在保留现有点动能力的基础上，增加触摸屏单笔采集、预览、软限位验证和 X/Y 双轴自动播放。

**架构：** 新增无硬件依赖的纯 C 轨迹核心，负责坐标映射、容量/行程校验及整数 DDA；GT9xx 向 UI 任务提供单指触摸事件。UI 将冻结后的静态轨迹提交给电机任务，电机任务独占运动命令，TIM2 更新中断按 DDA 脉冲掩码控制现有两路预装载 PWM。

**技术栈：** STM32F407 Cortex-M4、µC/OS-III、STM32F4 标准外设库、C99、CMake/arm-none-eabi-gcc、pytest + 主机 C 编译器测试。

**设计：** `docs/superpowers/specs/2026-10-08-one-stroke-drawing-design.md`

## 全局约束

- 软限位为 `X ∈ [-3500,3500]、Y ∈ [-2300,2300]`，任何一处越界都拒绝整笔，不裁切、不部分执行。
- 轨迹采样最多 2048 个 `int16_t` X/Y 步数点；固定静态存储，不使用堆。
- 每次播放前都要求用户重新确认机械中心；首个触摸点映射到该中心 (0,0)，无 Z/抬笔动作。
- 画布保持机械行程宽高比例；逻辑触屏向右为 +X、向上为 +Y，实际 DIR 极性必须在硬件上低速确认。
- 作画初始最高步频为 250 steps/s，TIM2 基础更新率为 1kHz；方向改变后等待至少 `MOTOR_DIR_SETUP_TICKS`。
- UI 不直接操作 TIM2/GPIO；TIM2 ISR 不阻塞、不画 LCD、不打印、不做 I²C。
- 整条轨迹在 `START` 前通过中心/状态/容量/软限位检查；播放时缓冲区只读；STOP 后不能残留会自动续动的脉冲或命令。
- 当前工作区没有 `.git`，实施期间不运行 `git worktree`、`git add` 或 `git commit`。

---

### Task 1：建立可主机测试的轨迹映射与 DDA 核心

**文件：**
- 创建：`User/plotter/plotter_path.h`
- 创建：`User/plotter/plotter_path.c`
- 创建：`tests/plotter_path_host.c`
- 创建：`tests/test_plotter_path.py`

**接口：**

```c
#define PLOTTER_MAX_POINTS       2048u
#define PLOTTER_X_LIMIT_STEPS    3500
#define PLOTTER_Y_LIMIT_STEPS    2300

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
    uint16_t origin_x_px;
    uint16_t origin_y_px;
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
PlotterPathState_t PlotterPath_AppendTouch(PlotterPath_t *path, uint16_t x, uint16_t y);
PlotterPathState_t PlotterPath_Finish(PlotterPath_t *path);
void PlotterPath_Reset(PlotterPath_t *path);

void PlotterDda_Begin(PlotterDda_t *dda, const PlotterPath_t *path);
uint8_t PlotterDda_Next(PlotterDda_t *dda,
                        uint8_t *axis_step_mask,
                        int8_t *dir_x,
                        int8_t *dir_y);
```

`PlotterPath_AppendTouch` 将第一个有效点存为 `(0,0)`；后续坐标按画布 `(left=24, top=220, width=433, height=285)` 与 7000:4600 比例换算为相对步数。连续相同步点不重复入列。越界和溢出状态是粘性的；只有状态为 `PLOTTER_PATH_READY` 且至少有两个点才能提交。`PlotterDda_Next` 每次返回某条折线的一个主轴步进事件，mask bit0 表示 X、bit1 表示 Y；`dir_x/dir_y` 为 -1/0/+1；返回 0 表示路径完成。

- [ ] **步骤 1：先写主机 C 断言，覆盖映射、软限位、溢出和 DDA。**

在 `tests/plotter_path_host.c` 写以下核心断言，并补齐四方向及反向段：

```c
PlotterCanvas_t canvas = {24u, 220u, 433u, 285u};
PlotterPath_t path;
PlotterPath_Begin(&path, &canvas);
assert(PlotterPath_AppendTouch(&path, 240u, 362u) == PLOTTER_PATH_CAPTURING);
assert(PlotterPath_AppendTouch(&path, 456u, 362u) == PLOTTER_PATH_CAPTURING);
assert(PlotterPath_AppendTouch(&path, 456u, 220u) == PLOTTER_PATH_CAPTURING);
assert(path.points[0].x_steps == 0 && path.points[0].y_steps == 0);
assert(path.points[1].x_steps == PLOTTER_X_LIMIT_STEPS);
assert(path.points[2].y_steps == PLOTTER_Y_LIMIT_STEPS);
assert(PlotterPath_Finish(&path) == PLOTTER_PATH_READY);

PlotterPath_Begin(&path, &canvas);
assert(PlotterPath_AppendTouch(&path, 24u, 362u) == PLOTTER_PATH_CAPTURING);
assert(PlotterPath_AppendTouch(&path, 240u, 362u) == PLOTTER_PATH_CAPTURING);
assert(PlotterPath_AppendTouch(&path, 456u, 362u) == PLOTTER_PATH_OUT_OF_BOUNDS);
assert(PlotterPath_Finish(&path) == PLOTTER_PATH_OUT_OF_BOUNDS);
```

另建有效 `(0,0)→(7,3)` 路径，循环 `PlotterDda_Next` 累计 mask，断言事件数为 7、X 步数为 7、Y 步数为 3、最终端点精确；测试 `(-7,-3)`、纯 X、纯 Y 和重复点。溢出用中心与左边缘两个不同点交替追加至 2048 点，再追加一次，断言状态变为 `PLOTTER_PATH_OVERFLOW` 且 `Finish` 不能变成 READY。

- [ ] **步骤 2：用 pytest 编译并运行主机 C 测试，确认先失败。**

`tests/test_plotter_path.py` 用 `subprocess.run` 执行：

```text
cc -std=c99 -Wall -Wextra -Werror -I User/plotter \
  User/plotter/plotter_path.c tests/plotter_path_host.c -o <pytest tmp_path>/plotter_path_host
<pytest tmp_path>/plotter_path_host
```

运行 `pytest -q tests/test_plotter_path.py`；预期在核心文件不存在时以编译错误失败。

- [ ] **步骤 3：实现最小纯 C 路径核心。**

创建头/源文件，所有映射运算使用有符号 32 位中间值和对称整数舍入；先检查 touch 在 canvas 内，再检查换算点的两个软限位；不调用 MCU、RTOS、LCD API。

- [ ] **步骤 4：重跑主机测试，再跑已有静态测试。**

运行 `pytest -q tests/test_plotter_path.py tests/test_static_contracts.py`；预期全部通过且 C 编译无警告。

---

### Task 2：把 GT9xx 最新坐标扩展为可消费的触摸事件

**文件：**
- 修改：`User/touch/gt9xx.h`
- 修改：`User/touch/gt9xx.c`
- 修改：`tests/test_static_contracts.py`

**接口：**

```c
typedef enum {
    GTP_EVENT_NONE = 0,
    GTP_EVENT_DOWN,
    GTP_EVENT_MOVE,
    GTP_EVENT_UP,
    GTP_EVENT_MULTI,
    GTP_EVENT_ERROR
} GTP_TouchEventType_t;

typedef struct {
    GTP_TouchEventType_t type;
    uint16_t x;
    uint16_t y;
    uint8_t touch_count;
} GTP_TouchEvent_t;

void GTP_TouchProcess(GTP_TouchEvent_t *event);
```

`GTP_Touch_Down(id,x,y,w,event)` 以 `pre_x[id] < 0` 区分 DOWN 与 MOVE 并填入坐标；`GTP_Touch_Up(id,event)` 填 UP 后清除该 ID 状态。若同一控制器报告无法无损表达多个边沿，优先报 ERROR/MULTI，不得伪造连续轨迹。

UI 在收到 GT9xx IRQ 标志后、任务上下文中调用该接口。DOWN/MOVE 事件携带已按 `LCD_SCAN_MODE` 转换的坐标；UP 有效结束当前单指轨迹；多指报告返回 MULTI；读写 I²C 失败返回 ERROR。保留 `GTP_GetTouchPoint` 旧接口，普通点动页仍按原方式工作。

- [ ] **步骤 1：先在静态契约测试中增加失败断言。**

增加 `test_gt9xx_exports_down_move_up_multi_and_error_events`：读 `gt9xx.h/.c`，断言事件枚举包含 DOWN/MOVE/UP/MULTI/ERROR、`GTP_TouchProcess` 接受事件输出参数、`GTP_Touch_Down` 能区分 DOWN/MOVE、`GTP_Touch_Up` 设置 UP、超过一个触点生成 MULTI、I²C 失败生成 ERROR，并确认触摸驱动事件处理函数没有 LCD 绘制调用。

- [ ] **步骤 2：运行单测确认失败。**

运行 `pytest -q tests/test_static_contracts.py::test_gt9xx_exports_down_move_up_multi_and_error_events`；预期因事件类型/接口尚不存在而失败。

- [ ] **步骤 3：实现事件提取并保留兼容坐标接口。**

把静态 `Goodix_TS_Work_Func` 改为向 `GTP_TouchProcess` 填充一次报告事件；用已有 `pre_touch` 与触点 ID 判断 DOWN/MOVE/UP；多点优先报 MULTI；读取/清状态失败时报 ERROR。不得从 ISR 调用 LCD 或运动接口。

- [ ] **步骤 4：运行触摸契约和既有测试。**

运行 `pytest -q tests/test_static_contracts.py`；预期全部通过。

---

### Task 3：在纯 C 核心和 TIM2 ISR 中实现有界双轴脉冲调度

**文件：**
- 修改：`User/motor/bsp_motor.h`
- 修改：`User/motor/bsp_motor.c`
- 修改：`User/stm32f4xx_it.c`（仅当 ISR 包装契约需调整时）
- 修改：`tests/test_static_contracts.py`

**接口/不变量：** `PLOTTER_STEP_RATE_HZ=250u`，基于 1kHz TIM2 更新做软件分频；DDA 产生 axis mask 后只给相应通道写 `MOTOR_PULSE_CCR`，未选中的通道写 0。每个更新事件先核对前一周期已生效的预装载 CCR，再更新已输出步数；STOP 标志到来后不再安排下一周期脉冲并在安全更新边界清零两通道。DIR 改变后至少等待 `MOTOR_DIR_SETUP_TICKS` 个 1ms 更新事件。

- [ ] **步骤 1：先增加电机 ISR 静态契约测试。**

增加 `test_motor_plotter_uses_bounded_timer_dda_and_no_blocking_calls`，提取 `Motor_UpdateISR` 函数体，断言它有作画 DDA/步频分频路径，使用 CH2/CH3 比较寄存器输出 mask，读取停止请求并清零两路 CCR；断言 ISR 函数体不含 `OSTimeDly`、`printf`、`NT35510_` 或 `I2C_` 调用。

- [ ] **步骤 2：运行单测确认失败。**

运行 `pytest -q tests/test_static_contracts.py::test_motor_plotter_uses_bounded_timer_dda_and_no_blocking_calls`；预期因现有 ISR 只有点动步数统计而失败。

- [ ] **步骤 3：接入 DDA 与脉冲调度。**

在 `bsp_motor.c` 中增加静态播放上下文、分频计数、方向建立计数、待发 CCR mask 和 stop/done 标志；只在 TIM2 更新 ISR 运行 DDA 的单个有界事件，不在 ISR 循环遍历整条路径。保持现有点动 PWM/计数行为不变。

- [ ] **步骤 4：跑 DDA 主机用例、ISR 契约和回归。**

运行 `pytest -q tests/test_plotter_path.py tests/test_static_contracts.py`；预期所有主机算法及静态契约通过。

---

### Task 4：让 Motor_Task 独占作画命令与点动互斥

**文件：**
- 修改：`User/task/motor_task.h`
- 修改：`User/task/motor_task.c`
- 修改：`User/motor/bsp_motor.h/.c`
- 修改：`tests/test_static_contracts.py`

**接口：**

```c
typedef enum {
    MOTOR_PLOT_IDLE = 0,
    MOTOR_PLOT_PENDING,
    MOTOR_PLOT_RUNNING,
    MOTOR_PLOT_DONE,
    MOTOR_PLOT_ABORTED,
    MOTOR_PLOT_REJECTED
} MotorPlotState_t;

uint8_t Motor_PlotterRequestStart(const PlotterPath_t *path);
void Motor_PlotterRequestStop(void);
MotorPlotState_t Motor_PlotterGetState(void);
uint16_t Motor_PlotterGetProgress(void);
```

UI 提交其静态 `PlotterPath_t` 指针和 READY 数据；提交后 UI 冻结该对象，直至 DONE/ABORTED/REJECTED。Motor_Task 消费命令前再次检查点动 IDLE、按键全松开和路径 READY；无效则 REJECTED。请求在电机启动前被拒绝时，用户修正原因后可以从 REJECTED 重试同一 READY 预览路径，因为机械位置尚未因作画而改变。PENDING 期间 STOP 与资格校验并发时，在同一临界区仲裁，已发布的 STOP 优先得到 ABORTED。播放期间不运行 `Jog_Update` 的启动/反向逻辑，ISR 是播放中唯一逐步推进路径的执行者。STOP 为幂等请求。

- [ ] **步骤 1：先增加命令互斥的静态契约测试。**

增加 `test_motor_task_serializes_jog_and_plotter_lifecycle`，断言公共 API 与状态枚举存在；`Motor_Task` 在每 10ms 周期处理作画命令；播放/停止期间屏蔽点动启动；启动检查路径 READY、无正在点动、按键均松开；DONE/ABORTED/REJECTED 后释放缓冲引用并清待发 CCR。

- [ ] **步骤 2：运行单测确认失败。**

运行 `pytest -q tests/test_static_contracts.py::test_motor_task_serializes_jog_and_plotter_lifecycle`；预期因作画命令接口不存在而失败。

- [ ] **步骤 3：实现任务/ISR 状态握手。**

使用短临界区保护 UI→Motor_Task 的命令指针/状态发布；Motor_Task 消费并初始化播放器，随后启动 TIM2 DDA；ISR 只写完成/中止标志，任务负责发布可供 UI 查询的最终状态。播放期间 `Key_Scan` 可更新状态，但 `Jog_Update` 不得起动新运动。

- [ ] **步骤 4：跑电机任务契约与回归测试。**

运行 `pytest -q tests/test_static_contracts.py tests/test_plotter_path.py`；预期通过。

---

### Task 5：集成作画页面、触摸采集、预览、开始与停止

**文件：**
- 修改：`User/ui/ui_task.c/.h`
- 修改：`User/ui/ui_draw.c/.h`
- 修改：`User/task/motor_task.h`（若 UI 状态接口需导出）
- 修改：`tests/test_static_contracts.py`

**界面/状态：**

```text
MOTOR_VIEW → CENTER_CONFIRM → CAPTURING → PREVIEW → PLAYING → RESULT
                                  ↘ INVALID          ↘ STOP → ABORTED
```

画布固定为 `(24,220,433,285)`，中心像素为 `(240,362)`，保留 7000:4600 机械比例。每个作画 UI 状态每不大于 10ms 轮询/处理触摸；普通页面继续 50ms 循环和既有 200ms 点击消抖。UI 创建静态 `PlotterPath_t`；在 `CENTER_CONFIRM` 之前不能采集，在 PREVIEW READY 之前不能调用 start API；播放期只接受 STOP。

- [ ] **步骤 1：先增加 UI 状态和不运动契约测试。**

增加 `test_ui_draw_flow_requires_center_valid_path_and_start`，断言主页面有 DRAW 命中区；作画状态包含 CENTER_CONFIRM/CAPTURING/PREVIEW/PLAYING/RESULT；center-ready 后 DOWN/MOVE 追加路径，UP 调 `PlotterPath_Finish`；Start 仅从有效 PREVIEW 提交；STOP 仅从 PLAYING 发停止请求；播放期间不处理轴选择；作画活动状态 delay 为 10ms，普通页保持 50ms。

- [ ] **步骤 2：运行单测确认失败。**

运行 `pytest -q tests/test_static_contracts.py::test_ui_draw_flow_requires_center_valid_path_and_start`；预期因当前 UI 没有绘图状态/入口而失败。

- [ ] **步骤 3：先绘制不运动的 UI 页面与状态转移。**

在 `ui_draw.c/.h` 添加主页面 DRAW 入口、中心确认提示、画布、中心标记和 path segment 绘制、预览与错误/运行按钮；在 `ui_task.c` 添加 enum 状态及事件路由，但此步 Start 只可进入状态，不启动电机，等待后续步骤接入。

- [ ] **步骤 4：增加触摸事件到采集和按钮状态的连接。**

UI 任务在 IRQ 标志后调用 `GTP_TouchProcess(&event)`；CAPTURING 中处理 DOWN/MOVE/UP/MULTI/ERROR，越出画布、I²C error、多指/路径失败立即锁定 INVALID；其他页面保留旧按钮点击语义。

- [ ] **步骤 5：连接播放提交、进度、STOP 和回中失效。**

READY 预览的 START 调 `Motor_PlotterRequestStart(&s_plotter_path)`；RUNNING 展示 `Motor_PlotterGetProgress()` 并提供 STOP；DONE/ABORTED 清除中心确认；REJECTED 表示引擎未运动，保留 READY 预览与中心确认供修正条件后重试；REDRAW 仅在尚未消费中心确认时保留本次中心；返回点动页不发任何运动命令。

- [ ] **步骤 6：运行 UI 契约、绘图算法和现有回归。**

运行 `pytest -q tests/test_static_contracts.py tests/test_plotter_path.py`；预期所有测试通过。

---

### Task 6：构建清单、说明文档与完整验收

**文件：**
- 修改：`CMakeLists.txt`
- 修改：`README.md`
- 修改：`tests/test_static_contracts.py`（仅补充构建清单约束）

- [ ] **步骤 1：先增加构建清单测试。**

增加 `test_plotter_core_is_built_and_usb_demos_stay_excluded`，断言 `${CMAKE_SOURCE_DIR}/User/plotter/plotter_path.c` 在 `USER_SOURCES`，且不把现有 USB 示例误加到可执行目标。

- [ ] **步骤 2：运行单测确认缺少源文件配置。**

运行 `pytest -q tests/test_static_contracts.py::test_plotter_core_is_built_and_usb_demos_stay_excluded`；若 CMake 还未添加路径，预期失败。

- [ ] **步骤 3：加入 CMake 源文件并更新 README。**

把纯 C 核心加入现有固件目标；README 补充页面流程、中心/首点语义、X/Y ± 行程、250 steps/s 默认速率、无回零/抬笔/失步反馈的限制，以及需实机确认方向。

- [ ] **步骤 4：运行完整主机测试。**

运行 `pytest -q`；预期全部通过，记录实际通过/失败数量。

- [ ] **步骤 5：执行交叉编译。**

运行 `./build.sh`；预期 `zhsx.elf/.hex/.bin` 生成成功且链接内存报告未超出链接脚本区域。若工具链路径失效，先使用当前 STM32Cube GNU 工具链定位实际 `arm-none-eabi-gcc`，再通过 CMake `-DCMAKE_C_COMPILER=...` 重建。

- [ ] **步骤 6：执行硬件验收前安全检查并记录未覆盖项。**

没有连接目标板时不得声称硬件通过。连接后按设计文档低速验证 X/Y 正负方向、STEP 脉宽/计数、DIR setup、STOP、软限位和 PA14/SWCLK 复位下烧录，再用矩形/对角线核对预览与实体方向/比例。

---

## 自审

- 设计文档第 4–7 节分别由 Task 5、Task 2、Task 3–4 覆盖；路径几何和容量由 Task 1 覆盖。
- 设计文档第 8 节主机验收由 Task 1–6 覆盖，交叉编译由 Task 6 覆盖，硬件验收明确保留为必须在目标硬件完成的门槛。
- 设计文档第 9 节涉及的 CMake、README、测试、UI、触摸、电机/任务与新 plotter 核心均在任务文件清单中。
- 工作区无 `.git`，因此所有“提交”步骤不适用；不执行任何 Git 写命令。
- 接口名称、结构类型、`PLOTTER_MAX_POINTS`、canvas 几何和 `MotorPlotState_t` 在任务间保持一致。
