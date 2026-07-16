import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        Node(
            package='wheeltec_nav2',
            executable='initpose_service',
            name='initpose_service',
            parameters=[{'use_sim_time': False}],
        ),
    ])
