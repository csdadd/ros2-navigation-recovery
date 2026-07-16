cmake_minimum_required(VERSION 3.8)
project(robot_navigation2)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD 17)

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav2_msgs REQUIRED)
find_package(nav2_util REQUIRED)
find_package(nav2_core REQUIRED)
find_package(pluginlib REQUIRED)
find_package(yaml-cpp REQUIRED)

# Generate interfaces
find_package(rosidl_default_generators REQUIRED)
rosidl_generate_interfaces(${PROJECT_NAME}
  "action/FollowWaypointsWithWait.action"
  DEPENDENCIES geometry_msgs
)

ament_export_dependencies(rosidl_default_runtime)

# Wait at waypoint plugin (must be first, as waypoint_follower depends on it)
add_library(wait_at_waypoint_with_time SHARED
  src/wait_at_waypoint_with_time.cpp
)
target_include_directories(wait_at_waypoint_with_time PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

ament_target_dependencies(wait_at_waypoint_with_time
  rclcpp
  rclcpp_lifecycle
  geometry_msgs
  nav2_util
  nav2_core
  pluginlib
)

# Waypoint follower library
add_library(waypoint_follower_with_wait SHARED
  src/waypoint_follower_with_wait.cpp
)
target_include_directories(waypoint_follower_with_wait PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(waypoint_follower_with_wait
  rclcpp
  rclcpp_action
  rclcpp_lifecycle
  rclcpp_components
  geometry_msgs
  nav2_msgs
  nav2_util
  nav2_core
  pluginlib
)
rosidl_target_interfaces(waypoint_follower_with_wait
  ${PROJECT_NAME} "rosidl_typesupport_cpp"
)
target_link_libraries(waypoint_follower_with_wait wait_at_waypoint_with_time)

# Executable

add_executable(initpose_service src/initpose_service.cpp)
ament_target_dependencies(initpose_service rclcpp geometry_msgs nav2_msgs)

add_executable(bt_log_monitor src/bt_log_monitor.cpp)
ament_target_dependencies(bt_log_monitor rclcpp nav2_msgs)

add_executable(waypoint_follower_with_wait_node src/main.cpp)
target_link_libraries(waypoint_follower_with_wait_node waypoint_follower_with_wait)
ament_target_dependencies(waypoint_follower_with_wait_node
  rclcpp
  rclcpp_action
  rclcpp_lifecycle
  rclcpp_components
  geometry_msgs
  nav2_msgs
  nav2_util
  nav2_core
  pluginlib
)

# Waypoint Runner Client Node
add_executable(waypoint_runner_client
  src/waypoint_runner_main.cpp
  src/waypoint_runner_client.cpp
)
target_include_directories(waypoint_runner_client PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(waypoint_runner_client
  rclcpp
  rclcpp_action
  geometry_msgs
  nav2_msgs
)
target_link_libraries(waypoint_runner_client yaml-cpp)
rosidl_target_interfaces(waypoint_runner_client
  ${PROJECT_NAME} "rosidl_typesupport_cpp"
)

# BT 行为树单元测试（可选构建）
if(BUILD_TESTING)
  find_package(custom_bt_plugins REQUIRED)
  add_executable(test_final_recovery
    test/test_final_recovery.cpp
  )
  target_include_directories(test_final_recovery PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  )
  ament_target_dependencies(test_final_recovery
    rclcpp
    std_msgs
    behaviortree_cpp_v3
    custom_bt_plugins
  )
  target_link_libraries(test_final_recovery custom_bt_plugins::publish_intervention_status)

  install(TARGETS test_final_recovery
    RUNTIME DESTINATION lib/${PROJECT_NAME}
  )
endif()

# Plugin export
pluginlib_export_plugin_description_file(nav2_core plugins/wait_at_waypoint_with_time.xml)

# Install
install(TARGETS wait_at_waypoint_with_time waypoint_follower_with_wait waypoint_follower_with_wait_node waypoint_runner_client
  initpose_service bt_log_monitor

  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY include/
  DESTINATION include
)

install(DIRECTORY launch maps config behavior_trees
  DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
