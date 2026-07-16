# AGENTS.md

## 项目概览

ROS2 (Humble/Jazzy) 多智能体导航系统毕业设计。由一个ROS2工作区的 4 个 ament_cmake 功能包组成，外加一个实机适配变体（galictic版本）。

## 功能包与构建顺序

所有包必须放在 ROS2 工作区 `src/` 下，使用 `colcon build` 构建。

### 仿真版（主）

```
custom_bt_plugins  →  robot_navigation2  →  robo_nav2_hmi
```

| 包名 | 说明 |
|------|------|
| `custom_bt_plugins` | BehaviorTree 自定义节点（IsFootprintInCollision, PublishInterventionStatus） |
| `robot_navigation2` | 航点导航 + 三级智能恢复策略 + 自定义 Action（FollowWaypointsWithWait） |
| `robo_nav2_hmi` | Qt5 人机交互界面（依赖 robot_navigation2 生成的 Action 接口） |
| `gazebo_simulation` | Gazebo 仿真环境（独立，无包间依赖） |

### galictic版本（实机版）

```
galictic版本恢复算法功能包/custom_bt_plugins  →  galictic版本恢复算法功能包/wheeltec_robot_nav2
```

**重要**：galictic 版本与仿真版的 `custom_bt_plugins` 包名相同（都叫 `custom_bt_plugins`），**不能同时放在同一个 ROS2 工作区**。galictic 版本比仿真版多一个 `move_forward_action` 插件和 `nav2_core` recovery 插件导出。

`wheeltec_robot_nav2` 的 ROS 包名为 `wheeltec_nav2`（非目录名），覆盖 30+ 种 Wheeltec 底盘型号的参数文件。

## 构建命令

```bash
# 单包构建
colcon build --packages-select custom_bt_plugins
colcon build --packages-select robot_navigation2
colcon build --packages-select robo_nav2_hmi

# 按依赖顺序构建（必须遵循上述顺序）
colcon build --packages-select custom_bt_plugins robot_navigation2 robo_nav2_hmi

# galictic 实机版（需先在 wheeltec 工控机上部署 ROS2 环境）
colcon build --packages-select custom_bt_plugins wheeltec_nav2
```

## 关键架构细节

- `robot_navigation2` 通过 `rosidl_generate_interfaces` 生成了自定义 Action `FollowWaypointsWithWait.action`，`robo_nav2_hmi` 依赖此接口，因此 `robot_navigation2` 必须先于 `robo_nav2_hmi` 构建。
- 行为树 XML（`behavior_trees/` 下）在运行时通过 Nav2 BT Navigator 加载，不在编译时校验。修改 XML 后无需重新编译。
- `robo_nav2_hmi` 使用 Qt5 + AUTOMOC/AUTOUIC/AUTORCC，设置 `WIN32_EXECUTABLE TRUE`（Windows 无控制台窗口）。
- 地图文件（PGM + YAML）、航点 YAML、Nav2 参数 YAML 通过 `install()` 安装到 `share/<pkg>/`，运行时由 launch 文件通过 `share` 目录加载。
- 日志系统和用户认证在 `robo_nav2_hmi` 中使用 SQLite 本地持久化。

## 测试

```bash
# 行为树单元测试（需要先构建 custom_bt_plugins）
colcon test --packages-select robot_navigation2 --event-handlers console_direct+
```

测试仅在 `BUILD_TESTING=ON`（默认）时编译，依赖 `custom_bt_plugins::publish_intervention_status` 链接库。

## 格式/检查

Lint 检查仅在 `BUILD_TESTING=ON` 时通过 `ament_lint_auto` 执行。各包使用 C++17，GCC/Clang 启用 `-Wall -Wextra -Wpedantic`。

## 原始说明

更详细的功能说明见 `源代码说明.txt`。行为树恢复策略的流程图和决策表见 `robot_navigation2/behavior_trees/行为树逻辑.md`。
