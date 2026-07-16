cmake_minimum_required(VERSION 3.8)
project(custom_bt_plugins)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD 17)

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(behaviortree_cpp_v3 REQUIRED)
find_package(nav2_costmap_2d REQUIRED)
find_package(nav2_util REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(nav2_behavior_tree REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(nav2_msgs REQUIRED)
find_package(pluginlib REQUIRED)
find_package(std_msgs REQUIRED)
find_package(nav2_core REQUIRED)

# IsFootprintInCollision plugin
add_library(is_footprint_in_collision SHARED
  src/is_footprint_in_collision.cpp
)

target_include_directories(is_footprint_in_collision PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

ament_target_dependencies(is_footprint_in_collision
  behaviortree_cpp_v3
  nav2_costmap_2d
  nav2_util
  geometry_msgs
  rclcpp
  tf2_ros
  nav2_behavior_tree
  tf2_geometry_msgs
  nav2_msgs
  pluginlib
)

target_compile_definitions(is_footprint_in_collision PRIVATE BT_PLUGIN_EXPORT)

# PublishInterventionStatus plugin
add_library(publish_intervention_status SHARED
  src/publish_intervention_status.cpp
)

target_include_directories(publish_intervention_status PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

ament_target_dependencies(publish_intervention_status
  behaviortree_cpp_v3
  rclcpp
  std_msgs
)

target_compile_definitions(publish_intervention_status PRIVATE BT_PLUGIN_EXPORT)

# MoveForwardAction plugin
add_library(move_forward_action SHARED
  src/move_forward_action.cpp
)

target_include_directories(move_forward_action PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

ament_target_dependencies(move_forward_action
  behaviortree_cpp_v3
  rclcpp
  nav2_behavior_tree
  nav2_msgs
)

target_compile_definitions(move_forward_action PRIVATE BT_PLUGIN_EXPORT)


# Install library
install(TARGETS
  is_footprint_in_collision
  publish_intervention_status
  move_forward_action
  EXPORT export_custom_bt_plugins
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

# Install include directory
install(DIRECTORY include/
  DESTINATION include
)

# Install plugin xml
install(FILES custom_bt_plugins_plugin.xml
  DESTINATION share/${PROJECT_NAME}
)


# Export for pluginlib
ament_export_targets(export_custom_bt_plugins HAS_LIBRARY_TARGET)
ament_export_dependencies(
  behaviortree_cpp_v3
  nav2_costmap_2d
  nav2_util
  rclcpp
  pluginlib
  std_msgs
  nav2_core
  nav2_msgs
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
