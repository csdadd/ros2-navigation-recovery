#include <rclcpp/rclcpp.hpp>
#include <nav2_msgs/msg/behavior_tree_log.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <filesystem>

class BTLogMonitor : public rclcpp::Node
{
public:
    BTLogMonitor() : Node("bt_log_monitor")
    {
        this->declare_parameter("log_file", "bt_log.txt");

        std::string log_file = this->get_parameter("log_file").as_string();

        log_file_.open(log_file, std::ios::out | std::ios::app);
        if (!log_file_.is_open())
        {
            RCLCPP_ERROR(this->get_logger(), "Failed to open log file: %s", log_file.c_str());
        }
        else
        {
            RCLCPP_INFO(this->get_logger(), "Logging to file: %s", log_file.c_str());
            auto now = std::chrono::system_clock::now();
            auto now_time = std::chrono::system_clock::to_time_t(now);
            log_file_ << "\n========================================\n";
            log_file_ << "BT Log Session Started: " << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S") << "\n";
            log_file_ << "========================================\n";
            log_file_.flush();
        }

        subscription_ = this->create_subscription<nav2_msgs::msg::BehaviorTreeLog>(
            "/behavior_tree_log",
            rclcpp::QoS(10),
            std::bind(&BTLogMonitor::log_callback, this, std::placeholders::_1));

        RCLCPP_INFO(this->get_logger(), "BT Log Monitor started - subscribing to /behavior_tree_log");
    }

    ~BTLogMonitor()
    {
        if (log_file_.is_open())
        {
            log_file_ << "\n========================================\n";
            log_file_ << "BT Log Session Ended\n";
            log_file_ << "========================================\n";
            log_file_.close();
        }
    }

private:
    void log_callback(const nav2_msgs::msg::BehaviorTreeLog::SharedPtr msg)
    {
        if (!log_file_.is_open())
        {
            return;
        }

        for (const auto & event : msg->event_log)
        {
            auto timestamp_sec = event.timestamp.sec;
            auto timestamp_nsec = event.timestamp.nanosec;
            double timestamp = static_cast<double>(timestamp_sec) + static_cast<double>(timestamp_nsec) * 1e-9;

            log_file_ << std::fixed << std::setprecision(3) << "[" << timestamp << "] ";
            log_file_ << std::setw(40) << std::left << event.node_name << ": ";
            log_file_ << std::setw(10) << std::left << event.previous_status;
            log_file_ << " -> ";
            log_file_ << std::setw(10) << std::left << event.current_status;
            log_file_ << "\n";
            log_file_.flush();
        }
    }

    rclcpp::Subscription<nav2_msgs::msg::BehaviorTreeLog>::SharedPtr subscription_;
    std::ofstream log_file_;
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<BTLogMonitor>());
    rclcpp::shutdown();
    return 0;
}
