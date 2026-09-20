#include<rclcpp/rclcpp.hpp>
#include<std_msgs/msg/string.hpp>
#include <memory>
#include <string>
using StringDate = std_msgs::msg::String;
class SubscriberDemo : public rclcpp::Node
{
    public:
    SubscriberDemo(const std::string node_name): Node(node_name)
    {
        sub_=this->create_subscription<StringDate>("/w1/chatter",10,[this](const StringDate & msg)->void{
        RCLCPP_INFO(this->get_logger(),"%s",msg.data.c_str());

        });
        RCLCPP_INFO(this->get_logger(),"订阅者已启动：正在监听 /w1/chatter");
    }


    private:
    rclcpp::Subscription<StringDate>::SharedPtr sub_; 
};
int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);                            // 1. 初始化 ROS2
    auto node = std::make_shared<SubscriberDemo>("w1_topic_sub");  // 2. 创建节点
    rclcpp::spin(node);                                  // 3. 转起来（一直处理回调）
    rclcpp::shutdown();                                  // 4. Ctrl+C 后关闭
    return 0;
}

