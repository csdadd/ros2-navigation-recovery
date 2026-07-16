import launch
from launch import event_handlers
import launch_ros
from ament_index_python.packages import get_package_share_directory
from launch.launch_description_sources import PythonLaunchDescriptionSource

def generate_launch_description():
    #获取默认地址
    robot_name_in_model = "robot"
    urdf_tutorial_path = get_package_share_directory('gazebo_simulation')
    default_world_path = urdf_tutorial_path + '/world/simulation_environment.world'

    # 默认 URDF 路径 (阿克曼转向机器人)
    default_model_path = urdf_tutorial_path + '/urdf/robot_ackermann/robot.urdf.xacro'

    #为 launch声明参数
    action_declare_arg_model_path = launch.actions.DeclareLaunchArgument(
        name='model',default_value=default_model_path,
        description='URDF的绝对路径')
    #获取文件内容生成新的参数
    robot_description = launch_ros.parameter_descriptions.ParameterValue(
        launch.substitutions.Command(
            ['xacro ', launch.substitutions.LaunchConfiguration('model')]),
            value_type=str)
    #状态发布节点
    robot_state_publisher_node = launch_ros.actions.Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        parameters=[{'robot_description':robot_description},
                    {'use_sim_time': True}],
    )
    #通过 IncludeLaunchDescription 包含另外一个 launch 文件
    launch_gazebo = launch.actions.IncludeLaunchDescription(
        PythonLaunchDescriptionSource([get_package_share_directory(
                'gazebo_ros'), '/launch', '/gazebo.launch.py']),
            #传递参数
        launch_arguments=[('world', default_world_path),('verbose','true')]
    )
    #请求 Gazebo 加载机器人
    spawn_entity_node = launch_ros.actions.Node(
        package='gazebo_ros',
        executable='spawn_entity.py',
        arguments=['-topic', '/robot_description', '-entity', robot_name_in_model, '-x', '0.4', '-y', '0.4', '-z', '0.05']
    )
    #加载并激活 robot_joint_state_broadcaster 控制器
    load_joint_state_controller = launch.actions.ExecuteProcess(
        cmd=['ros2', 'control', 'load_controller', '--set-state', 'active',
                'robot_joint_state_broadcaster'],
        output='screen'
    )
    #加载并激活 robot_effort_controller 控制器
    load_effort_controller = launch.actions.ExecuteProcess(
        cmd=['ros2', 'control', 'load_controller', '--set-state', 'active',
                'robot_effort_controller'],
        output='screen'
    )
    # 加载并激活阿克曼控制器
    load_controller = launch.actions.ExecuteProcess(
        cmd=['ros2', 'control', 'load_controller', '--set-state', 'active',
                'robot_ackermann_controller'],
        output='screen'
    )

    return launch.LaunchDescription([
        action_declare_arg_model_path,
        robot_state_publisher_node,
        launch_gazebo,
        spawn_entity_node,
        launch.actions.RegisterEventHandler(
            event_handler=launch.event_handlers.OnProcessExit(
                target_action=spawn_entity_node,
                on_exit=[load_joint_state_controller],
        )),
        launch.actions.RegisterEventHandler(
            event_handler=launch.event_handlers.OnProcessExit(
                target_action=load_joint_state_controller,
                on_exit=[load_effort_controller],
        )),
        launch.actions.RegisterEventHandler(
            event_handler=launch.event_handlers.OnProcessExit(
                target_action=load_effort_controller,
                on_exit=[load_controller],
        )),
    ])
