// 测试 FinalRecoveryFallback: 验证 ForceFailure 立即结束而非等待 60s
#include "behaviortree_cpp_v3/bt_factory.h"
#include "behaviortree_cpp_v3/loggers/bt_cout_logger.h"
#include "custom_bt_plugins/publish_intervention_status.hpp"
#include "rclcpp/rclcpp.hpp"

#include <unistd.h>

// GoalUpdated stub — 始终返回 FAILURE（模拟无新目标到达）
class GoalUpdatedStub : public BT::ConditionNode
{
public:
  GoalUpdatedStub(const std::string & name, const BT::NodeConfiguration & config)
  : BT::ConditionNode(name, config)
  {
  }
  static BT::PortsList providedPorts() { return BT::PortsList{}; }
  BT::NodeStatus tick() override
  {
    printf("[GoalUpdated] -> FAILURE\n");
    return BT::NodeStatus::FAILURE;
  }
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = rclcpp::Node::make_shared("bt_final_recovery_test");

  BT::BehaviorTreeFactory factory;

  // 注册 stub
  factory.registerNodeType<GoalUpdatedStub>("GoalUpdated");

  // 注册真实的 PublishInterventionStatus（来自 custom_bt_plugins）
  factory.registerNodeType<custom_bt_plugins::PublishInterventionStatus>(
    "PublishInterventionStatus");

  // 将 ROS node 放入黑板（PublishInterventionStatus 需要它来发布话题）
  auto bb = BT::Blackboard::create();
  bb->set("node", node);

  // 从 XML 创建行为树
  std::string xml_path = (argc >= 2) ? argv[1] :
    "src/robot_navigation2/test/test_final_recovery_fallback.xml";
  auto tree = factory.createTreeFromFile(xml_path, bb);

  // 附加日志器，打印每次 tick 的状态变化
  BT::StdCoutLogger logger(tree);

  printf("\n=== 开始 tick 行为树 ===\n");
  printf("预期: Publish 成功后, ForceFailure 强制 FAILURE, 树终止\n\n");

  BT::NodeStatus status = BT::NodeStatus::RUNNING;
  int tick_count = 0;
  const int max_ticks = 60;  // 最多 60 tick，正常应远少于此

  while (status == BT::NodeStatus::RUNNING && tick_count < max_ticks && rclcpp::ok()) {
    tick_count++;
    printf("--- Tick %d ---\n", tick_count);
    status = tree.tickRoot();
    rclcpp::spin_some(node);
    usleep(100000);  // 100ms，给 RetryUntilSuccessful 重试间隔
  }

  printf("\n=== 结果 ===\n");
  printf("Tick 次数: %d\n", tick_count);
  printf("最终状态: %s\n", BT::toStr(status).c_str());

  if (status == BT::NodeStatus::FAILURE) {
    printf("✓ 通过: ForceFailure 正确终止了行为树（未等待 60s）\n");
    rclcpp::shutdown();
    return 0;
  } else if (status == BT::NodeStatus::RUNNING) {
    printf("✗ 失败: 行为树仍在 RUNNING（类似旧版 Wait 60s 不终止）\n");
    rclcpp::shutdown();
    return 1;
  } else {
    printf("? 意外: 状态 = %s\n", BT::toStr(status).c_str());
    rclcpp::shutdown();
    return 1;
  }
}
