import os
import launch
from ament_index_python.packages import get_package_share_directory
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from nav2_common.launch import RewrittenYaml


def generate_launch_description():
    """阿克曼机器人导航启动配置，不包含rviz2，强制使用仿真时间，使用自定义恢复行为树"""
    # 获取与拼接默认路径
    robot_navigation2_dir = get_package_share_directory('robot_navigation2')
    nav2_bringup_dir = get_package_share_directory('nav2_bringup')

    # 创建 Launch 配置变量
    use_sim_time = LaunchConfiguration('use_sim_time')
    map_yaml_path = LaunchConfiguration('map')
    nav2_param_path = LaunchConfiguration('params_file')
    bt_xml_path = LaunchConfiguration('bt_xml_file')

    # 使用 RewrittenYaml 动态替换行为树路径
    param_substitutions = {
        'use_sim_time': use_sim_time,
        'default_nav_to_pose_bt_xml': bt_xml_path,
        'default_nav_through_poses_bt_xml': bt_xml_path}

    configured_params = RewrittenYaml(
        source_file=nav2_param_path,
        param_rewrites=param_substitutions,
        convert_types=True)

    return launch.LaunchDescription([
        # 声明 Launch 参数
        DeclareLaunchArgument(
            'use_sim_time', default_value='true',
            description='Use simulation (Gazebo) clock if true'),
        DeclareLaunchArgument(
            'map', default_value=os.path.join(robot_navigation2_dir, 'maps', 'simulationMap.yaml'),
            description='Full path to map file to load'),
        DeclareLaunchArgument(
            'params_file', default_value=os.path.join(robot_navigation2_dir, 'config', 'nav2_params_ackermann.yaml'),
            description='Full path to param file to load'),
        DeclareLaunchArgument(
            'bt_xml_file', default_value=os.path.join(robot_navigation2_dir, 'behavior_trees', 'intelligent_recovery_ackermann.xml'),
            description='Full path to behavior tree xml file to load'),

        launch.actions.IncludeLaunchDescription(
            PythonLaunchDescriptionSource([nav2_bringup_dir, '/launch', '/bringup_launch.py']),
            launch_arguments={
                'map': map_yaml_path,
                'use_sim_time': use_sim_time,
                'params_file': configured_params}.items(),
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

        # 行为树日志监控节点 - 实时输出当前运行的行为树节点状态
        Node(
            package='robot_navigation2',
            executable='bt_log_monitor',
            name='bt_log_monitor',
            output='screen',
            parameters=[{'use_sim_time': use_sim_time}],
        ),
    ])
