#include<rclcpp/rclcpp.hpp>
#include<example_interfaces/srv/add_two_ints.hpp>
#include<chrono>

using ADDInterface =example_interfaces::srv::AddTwoInts;
using namespace std::chrono_literals;

class ADDTwoClinet :public rclcpp::Node
{
    public:
    ADDTwoClinet(const std::string & node_name):Node(node_name)
    {
        add_clinet=this->create_client<ADDInterface>("/w1/service");
        timer_=this->create_wall_timer(2s,[this]()->void{
            if (!add_clinet->wait_for_service(1s))
            {
                RCLCPP_INFO(this->get_logger(),"服务还是没有上线");
                if (!rclcpp::ok())
                {
                    RCLCPP_INFO(this->get_logger(),"rcl程序挂掉了,我退下了");
                }
                return;
            }//到这里其实我的思维因为最近上的plc课程给污染了，和plc不同，c/c+语言是顺序编程，是顺序结构进行，只要if还没有结束，我的程序是一直卡住的，不是状态机器
            auto request =std::make_shared<ADDInterface::Request>();
            request->a=++a_;//之前踩过一次坑，cline已经给我写过笔记，如果是a++，那么第一次会从0开始
            request->b=100;
            add_clinet->async_send_request(request,[this](const rclcpp::Client<ADDInterface>::SharedFuture future)->void{
                 auto get_respond=future.get();
                 RCLCPP_INFO(this->get_logger(), "收到响应:sum = %ld", get_respond->sum);
            }); //类似于广播的发送，所以这也是为什么说实际上服务，也是由话题组成的原因
//疑问：我发现函数SharedFutureAndRequestId的返回值是SharedFutureAndRequestId，但是我好像并不知道这个的用法。
        });

    }

    private:
    rclcpp::Client<ADDInterface>::SharedPtr add_clinet;
    rclcpp::TimerBase::SharedPtr timer_;
    int32_t a_{0};
};

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);                           // 1. 初始化 ROS2
    auto node = std::make_shared<ADDTwoClinet>("w1_client");  // 2. 创建节点
    rclcpp::spin(node);                                 // 3. 转起来
    rclcpp::shutdown();                                 // 4. 关闭
    return 0;
}