cmake_minimum_required(VERSION 3.5)

project(robo_nav2_hmi VERSION 0.1 LANGUAGES CXX)

if(DEFINED CATKIN_INSTALL_INTO_PREFIX_ROOT)
  unset(CATKIN_INSTALL_INTO_PREFIX_ROOT)
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_VERSION VERSION_LESS "3.16")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

find_package(ament_cmake REQUIRED)
find_package(ament_index_cpp REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Widgets Concurrent Sql Test)

find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(diagnostic_msgs REQUIRED)
find_package(action_msgs REQUIRED)
find_package(nav2_msgs REQUIRED)
find_package(rcl_interfaces REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(yaml-cpp REQUIRED)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        basethread.h
        basethread.cpp
        roscontextmanager.h
        roscontextmanager.cpp
        robotstatusthread.h
        robotstatusthread.cpp
        systemmonitorthread.h
        systemmonitorthread.cpp
        logthread.h
        logthread.cpp
        logtablemodel.h
        logtablemodel.cpp
        logfilterproxymodel.h
        logfilterproxymodel.cpp
        logstorageengine.h
        logstorageengine.cpp
        logquerytask.h
        logquerytask.cpp
        historylogmodel.h
        historylogmodel.cpp
        threadsafequeue.h
        mapconverter.h
        mapconverter.cpp
        maploader.h
        maploader.cpp
        mapcache.h
        mapcache.cpp
        mapmarker.h
        mapmarker.cpp
        statusindicator.h
        statusindicator.cpp
        navigationactionthread.h
        navigationactionthread.cpp
        pathvisualizer.h
        pathvisualizer.cpp
        nav2viewwidget.h
        nav2viewwidget.cpp
        nav2viewdataprocessor.h
        nav2viewdataprocessor.cpp
        user.h
        userstorageengine.h
        userstorageengine.cpp
        userauthmanager.h
        userauthmanager.cpp
        logindialog.h
        logindialog.cpp
        logindialog.ui
        usermanagementdialog.h
        usermanagementdialog.cpp
        usermanagementdialog.ui
        changepassworddialog.h
        changepassworddialog.cpp
        changepassworddialog.ui
        adduserdialog.h
        adduserdialog.cpp
        adduserdialog.ui
        nav2parameterthread.h
        nav2parameterthread.cpp
        geometryutils.h
        timeutils.h
        logutils.h
        coordinatetransformer.h
        coordinatetransformer.cpp
        renderdata.h
)

add_executable(robo_nav2_hmi
    ${PROJECT_SOURCES}
)

target_link_libraries(robo_nav2_hmi PRIVATE
    Qt5::Widgets
    Qt5::Concurrent
    Qt5::Sql
    yaml-cpp
)

ament_target_dependencies(robo_nav2_hmi PUBLIC
    rclcpp
    rclcpp_action
    std_msgs
    geometry_msgs
    nav_msgs
    sensor_msgs
    diagnostic_msgs
    action_msgs
    nav2_msgs
    rcl_interfaces
    tf2
    tf2_ros
    tf2_geometry_msgs
    ament_index_cpp
)

set_target_properties(robo_nav2_hmi PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

install(
  DIRECTORY map/
  DESTINATION share/${PROJECT_NAME}/map
)
install(
  TARGETS
  robo_nav2_hmi
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
