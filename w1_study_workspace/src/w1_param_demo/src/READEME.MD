问题1：
这个源码get_parameter(sub_name, parameter_variant);的result我第一次的时候还没有赋值，并且return result;返回数值的时候也没有说result是0,还是1,难道bool类型的默认是1？
Node::get_parameter(const std::string & name, ParameterT & parameter) const
{
  std::string sub_name = extend_name_with_sub_namespace(name, this->get_sub_namespace());

  rclcpp::Parameter parameter_variant;

  bool result = get_parameter(sub_name, parameter_variant);
  if (result) {
    parameter = static_cast<ParameterT>(parameter_variant.get_value<ParameterT>());
  }

  return result;
}
this->declare_parameter<std::string>("robot_name","fishbot");
this->declare_parameter<double>("max_speed",1.0);
this->declare_parameter<int64_t>("report_period_ms",2000);
//获取参数数值
this->get_parameter("robot_name",);
既然我第一次还没有给result赋值，那么我的 parameter = static_cast<ParameterT>(parameter_variant.get_value<ParameterT>());这一部没办法完成，但是this->get_parameter("robot_name",robot_name_);这个为什么又能够起到我想要的效果呢；
yin@yin-computer:~$ ros2 param get /param_node robot_name
String value is: fishbot
yin@yin-computer:~$ ros2 param get /param_node max_speed
Double value is: 2.0
yin@yin-computer:~$ ros2 param get /param_node report_period_ms
Integer value is: 3000
我尝试调用，发现确实第一次修改是起作用的，那么这是为什么呢

问题2：
我之前在学习的时候，我就在思考
 param_onback = this->add_on_set_parameters_callback(
        [this](const std::vector<rclcpp::Parameter>& paramters) -> rcl_interfaces::msg::SetParametersResult {
            rcl_interfaces::msg::SetParametersResult result;
            result.successful = true;  // 默认允许修改（改成 false 就是「拒绝」）
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
              report_period_ms_ = report_period_ms_;
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
        这段代码的result有什么作用,我尝试把result改成0尝试一下
        ![alt text](<截图 2026-09-24 22-16-32.png>) 地址在/home/yin/图片/截图证明我的return的数值确实会改变我result确实会直接改变我的修改结果
        
        ！那好似不是说我的知识点应该可以串起来了，我的问题1和问题2或许从来就不是分割的一部分，或许我问题2加入的回调函数就是给回调函数中会用到的？
        意思也就是说，我写的参数回调函数实际上我的服务的set_param的回调函数中的回调函数，类似于嵌套函数
        但是add_on_set_parameters_callback的result和get_parameter的result怎么关联起来呢？