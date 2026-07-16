import os
import launch
from ament_index_python.packages import get_package_share_directory
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node


def generate_launch_description():
    """生成差速构型 (Differential Drive) 导航启动描述

    机器人构型: 差速驱动
    - 支持 Spin 原地旋转恢复行为
    - 禁用横向速度
    - 使用差速专用 Nav2 参数配置和行为树

    不包含 rviz2，强制使用仿真时间，使用自定义恢复行为树
    """
    # 获取与拼接默认路径
    robot_navigation2_dir = get_package_share_directory(
        'robot_navigation2')
    nav2_bringup_dir = get_package_share_directory('nav2_bringup')

    # 创建 Launch 配置,默认使用仿真时间
    use_sim_time = launch.substitutions.LaunchConfiguration(
        'use_sim_time', default='true')
    map_yaml_path = launch.substitutions.LaunchConfiguration(
        'map', default=os.path.join(robot_navigation2_dir, 'maps', 'simulationMap.yaml'))
    # 差速专用参数文件
    nav2_param_path = launch.substitutions.LaunchConfiguration(
        'params_file', default=os.path.join(robot_navigation2_dir, 'config', 'nav2_params_differential.yaml'))
    # 差速专用行为树路径（包含 EscapeToSafeZone 恢复节点）
    bt_xml_path = launch.substitutions.LaunchConfiguration(
        'bt_xml_file', default=os.path.join(robot_navigation2_dir, 'behavior_trees', 'intelligent_recovery_differential.xml'))

    return launch.LaunchDescription([
        # 声明新的 Launch 参数
        launch.actions.DeclareLaunchArgument('use_sim_time', default_value=use_sim_time,
                                             description='Use simulation (Gazebo) clock if true'),
        launch.actions.DeclareLaunchArgument('map', default_value=map_yaml_path,
                                             description='Full path to map file to load'),
        launch.actions.DeclareLaunchArgument('params_file', default_value=nav2_param_path,
                                             description='Full path to param file to load'),
        launch.actions.DeclareLaunchArgument('bt_xml_file', default_value=bt_xml_path,
                                             description='Full path to behavior tree xml file to load'),

        launch.actions.IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                [nav2_bringup_dir, '/launch', '/bringup_launch.py']),
            # 使用 Launch 参数替换原有参数
            launch_arguments={
                'map': map_yaml_path,
                'use_sim_time': use_sim_time,
                'params_file': nav2_param_path,
                'default_nav_to_pose_bt_xml': bt_xml_path}.items(),
        ),

        # 启动带等待的多点导航action服务器
        Node(
            package='robot_navigation2',
            executable='waypoint_follower_with_wait_node',
            name='waypoint_follower_with_wait',
            output='screen',
            parameters=[{'use_sim_time': use_sim_time}],
        ),

        # 独立的 lifecycle_manager 管理 waypoint_follower_with_wait
        Node(
            package='nav2_lifecycle_manager',
            executable='lifecycle_manager',
            name='lifecycle_manager_waypoint_with_wait',
            output='screen',
            parameters=[{'use_sim_time': use_sim_time},
                        {'autostart': True},
                        {'node_names': ['waypoint_follower_with_wait']}],
        ),
    ])
