import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration

def generate_launch_description():
    config_dir = get_package_share_directory('wheeltec_nav2')
    waypoints_file = LaunchConfiguration('waypoints_file',
        default=os.path.join(config_dir, 'config', 'waypoints.yaml'))

    return LaunchDescription([
        DeclareLaunchArgument(
            'waypoints_file',
            default_value=waypoints_file,
            description='Path to waypoints YAML file'
        ),
        Node(
            package='wheeltec_nav2',
            executable='waypoint_follower_with_wait_node',
            name='waypoint_follower_with_wait',
            parameters=[{'use_sim_time': False}],
        ),
    ])
