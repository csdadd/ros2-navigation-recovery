cmake_minimum_required(VERSION 3.5)
project(wheeltec_nav2)

set(CMAKE_CXX_STANDARD 17)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()
add_compile_options(-g)

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(std_msgs REQUIRED)
find_package(behaviortree_cpp_v3 REQUIRED)
find_package(nav2_core REQUIRED)
find_package(pluginlib REQUIRED)

# Publish Intervention Status BT Node
add_library(publish_intervention_status SHARED
  src/publish_intervention_status.cpp
)
target_include_directories(publish_intervention_status PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_definitions(publish_intervention_status PRIVATE BT_PLUGIN_EXPORT)
ament_target_dependencies(publish_intervention_status
  rclcpp
  std_msgs
  behaviortree_cpp_v3
)

# Plugin export
pluginlib_export_plugin_description_file(nav2_behavior_tree plugins/publish_intervention_status.xml)

# Install targets
install(TARGETS publish_intervention_status
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

# Install directories
install(DIRECTORY include/
  DESTINATION include
)

install(DIRECTORY launch map param rviz action behavior_trees config
  DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
