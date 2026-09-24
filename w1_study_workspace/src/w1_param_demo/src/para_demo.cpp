#include <rclcpp/rclcpp.hpp>
#include <rcl_interfaces/msg/set_parameters_result.hpp>

#include <chrono>
#include <string>
#include <vector>
#include <memory>

class ParamNode : public rclcpp::Node
{
public:
  ParamNode(const std::string& node_name) : Node(node_name)
  {
    //声明参数
    this->declare_parameter<std::string>("robot_name", "fishbot");
    this->declare_parameter<double>("max_speed", 2.0);
    this->declare_parameter<int64_t>("report_period_ms", 3000);
    //为了验证我的reademe的问题，我将几个参数的初始数值改变以下
    //获取参数数值
    this->get_parameter("robot_name", robot_name_);
    this->get_parameter("max_speed", max_speed_);
    this->get_parameter("report_period_ms", report_period_ms_);

    //写参数回调部分
    param_onback = this->add_on_set_parameters_callback(
        [this](const std::vector<rclcpp::Parameter>& paramters) -> rcl_interfaces::msg::SetParametersResult {
            rcl_interfaces::msg::SetParametersResult result;
            result.successful =1;  // 默认允许修改（改成 false 就是「拒绝」）
          for (const auto& para : paramters)
          {
            if (para.get_name() == "robot_name")
            {
              robot_name_ = para.as_string();
            }
            else if (para.get_name() == "max_speed")
            {
              max_speed_ = para.as_double();
            }
            else if (para.get_name() == "report_period_ms")
            {
              report_period_ms_ = para.as_int();
              // 周期变了：重建定时器（定时器周期不能在运行时直接改）
              timer_ = this->create_wall_timer(std::chrono::milliseconds(report_period_ms_), [this]() -> void {
                RCLCPP_INFO(this->get_logger(), "[巡检] robot_name=%s, max_speed=%.2f, report_period_ms=%ld",
                            robot_name_.c_str(), max_speed_, report_period_ms_);
              });
              RCLCPP_INFO(this->get_logger(), "打印周期已改为 %ld ms", report_period_ms_);
            }
          }
          return result;
        });
        timer_ = this->create_wall_timer(std::chrono::milliseconds(report_period_ms_),
                                         [this]()->void{   RCLCPP_INFO(this->get_logger(), "[巡检] robot_name=%s, max_speed=%.2f, report_period_ms=%ld", robot_name_.c_str(),
                max_speed_, report_period_ms_);});
  }

private:
  rclcpp::node_interfaces::OnSetParametersCallbackHandle::SharedPtr param_onback;
  rclcpp::TimerBase::SharedPtr timer_;

  std::string robot_name_{ "fishbot" };
  double max_speed_{ 1.0 };
  int64_t report_period_ms_{ 2000 };
};

int main(int argc, char** argv)
{
  rclcpp::init(argc, argv);                   // 1. 初始化 ROS2
  auto node = std::make_shared<ParamNode>("param_node");  // 2. 创建节点
  rclcpp::spin(node);                         // 3. 转起来
  rclcpp::shutdown();                         // 4. 关闭
  return 0;
}
