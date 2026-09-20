#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
using StringDate = std_msgs::msg::String;
using namespace std::chrono_literals;

class TopicPublishDemo : public rclcpp::Node
{
public:
  TopicPublishDemo(const std::string node_name) : Node(node_name)
  {
    pub_ = this->create_publisher<StringDate>("/w1/chatter", 10);
    tim_ = this->create_wall_timer(1s, [this]() -> void {
      StringDate msg;
      msg.data = "现在已经发送消息第" + std::to_string(count++) + "条";
      pub_->publish(msg);
    });
  }

private:
  rclcpp::Publisher<StringDate>::SharedPtr pub_;
  rclcpp::TimerBase::SharedPtr tim_;
  int32_t count{ 0 };
};

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);                            // 1. 初始化 ROS2
    auto node = std::make_shared<TopicPublishDemo>("w1_topic_publish");  // 2. 创建节点
    rclcpp::spin(node);                                  // 3. 转起来（一直处理回调）
    rclcpp::shutdown();                                  // 4. Ctrl+C 后关闭
    return 0;
}
