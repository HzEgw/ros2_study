#include<rclcpp/rclcpp.hpp>
#include<example_interfaces/srv/add_two_ints.hpp>
using ADDInterface =example_interfaces::srv::AddTwoInts;

class ADDTwoService :public rclcpp::Node
{
public:
ADDTwoService(const std::string & node_name) :Node(node_name)
{

    ser_=this->create_service<ADDInterface>("/w1/service",[this](const ADDInterface::Request::SharedPtr &req,ADDInterface::Response::SharedPtr res)->void{
    res->sum=req->a+req->b;
    RCLCPP_INFO(this->get_logger(), "收到请求：%ld + %ld = %ld", req->a, req->b, res->sum);
        //这里我尝试了一次lambda函数传入的数值不是
    });
}



private:
rclcpp::Service<ADDInterface>::SharedPtr ser_;
};
int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);                           // 1. 初始化 ROS2
    auto node = std::make_shared<ADDTwoService>("w1_service");  // 2. 创建节点
    rclcpp::spin(node);                                 // 3. 转起来
    rclcpp::shutdown();                                 // 4. 关闭
    return 0;
}
