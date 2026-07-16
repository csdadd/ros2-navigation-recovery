import os
import launch
import launch_ros
from ament_index_python.packages import get_package_share_directory


def generate_launch_description():
    """生成rviz2启动描述,强制使用仿真时间"""
    # 获取与拼接默认路径
    nav2_bringup_dir = get_package_share_directory('nav2_bringup')
    rviz_config_dir = os.path.join(
        nav2_bringup_dir, 'rviz', 'nav2_default_view.rviz')

    # 创建 Launch 配置,默认使用仿真时间
    use_sim_time = launch.substitutions.LaunchConfiguration(
        'use_sim_time', default='true')

    return launch.LaunchDescription([
        # 声明 Launch 参数
        launch.actions.DeclareLaunchArgument('use_sim_time', default_value=use_sim_time,
                                             description='Use simulation (Gazebo) clock if true'),

        # 启动 rviz2 节点
        launch_ros.actions.Node(
            package='rviz2',
            executable='rviz2',
            name='rviz2',
            arguments=['-d', rviz_config_dir],
            parameters=[{'use_sim_time': use_sim_time}],
            output='screen'),
    ])
