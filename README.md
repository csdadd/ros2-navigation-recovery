# ROS2 Multi-Agent Navigation System

ROS2 (Humble/Jazzy) multi-agent navigation system with intelligent recovery strategies for undergraduate thesis.

## Packages

| Package | Description |
|---------|-------------|
| `custom_bt_plugins` | Custom BehaviorTree nodes (IsFootprintInCollision, PublishInterventionStatus) |
| `robot_navigation2` | Waypoint navigation with 3-tier intelligent recovery + custom Action |
| `robo_nav2_hmi` | Qt5-based human-machine interface with multi-threaded architecture |
| `gazebo_simulation` | Gazebo simulation environment (Ackermann + Differential) |
| `galictic*/` | Real-robot adaptation for Wheeltec chassis (30+ models) |

## Intelligent Recovery Strategy (3-Tier)

On FollowPath failure, the Behavior Tree evaluates:

1. **Stuck branch**: Robot deceleration detected → BackUp
2. **Collision branch**: Footprint overlaps costmap obstacle → ClearCostmap → BackUp → ComputePathToPose → DriveOnHeading fallback
3. **Unknown obstacle branch**: Invisible obstacle (glass, corner) → BackUp → DriveOnHeading

If all recovery fails → falls back to manual intervention alert.

## Build

```bash
# Place packages under ROS2 workspace src/
colcon build --packages-select custom_bt_plugins robot_navigation2 robo_nav2_hmi
# gazebo_simulation is independent
colcon build --packages-select gazebo_simulation
```

## Test

```bash
colcon test --packages-select robot_navigation2 --event-handlers console_direct+
```

## Run

1. Launch Gazebo simulation (or real robot driver)
2. Launch Nav2 navigation stack
3. Launch behavior tree log monitor (bt_log_monitor)
4. Launch HMI for visualization and control

## License

Apache 2.0
