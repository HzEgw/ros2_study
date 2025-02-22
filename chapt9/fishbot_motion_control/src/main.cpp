#include <Arduino.h>
#include <Esp32PcntEncoder.h>
#include <Esp32McpwmMotor.h>
#include <PidController.h>
#include <Kinematics.h>

// 引入Microros和wifi相关的库
#include <WiFi.h>
#include <micro_ros_platformio.h>
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <geometry_msgs/msg/twist.h> // 消息接口
#include <nav_msgs/msg/odometry.h> // 里程计消息接口
#include <micro_ros_utilities/string_utilities.h> // 引入字符串内存分配初始化工具

// 声明一些相关的结构体对象
rcl_allocator_t allocator; // 内存分配器，用于动态内存分配管理
rclc_support_t support;    // 用于存储时钟，内存分配器和上下文，用于提供支持
rclc_executor_t executor;  // 执行器，用于管理订阅和计时器回调的执行
rcl_node_t node;           // 节点，用于创建节点
rcl_subscription_t sub_cmd_vel; // 创建一个订阅者
geometry_msgs__msg__Twist msg_cmd_vel; // 订阅到的数据存储到这里

rcl_publisher_t pub_odom; // 创建一个里程计发布者
nav_msgs__msg__Odometry msg_odom; // 里程计消息存储到这里
rcl_timer_t timer; // 定时器，可以定时调用某个函数




Esp32PcntEncoder encoders[2]; // 创建一个数组用于存储两个编码器
Esp32McpwmMotor motor;        // 创建一个名为motor的对象，用于控制电机
PidController pid_controller[2];
Kinematics kinematics;

float target_linear_speed = 0.0;  // 单位 毫米每秒
float target_angular_speed = 0.0; // 单位 弧度每秒
float out_left_speed = 0.0;       // 输出的左右轮速度，不是反馈的左右轮速度
float out_right_speed = 0.0;

// 定时器的回调函数
void timer_callback(rcl_timer_t* timer,int64_t last_call_time)
{
    // 完成里程计的发布
    odom_t  odom = kinematics.get_odom(); // 获取当前的里程计
    int64_t stamp = rmw_uros_epoch_millis();  // 获取当前的时间
    msg_odom.header.stamp.sec = static_cast<int32_t>(stamp/1000); // 秒部分
    msg_odom.header.stamp.nanosec = static_cast<int32_t>((stamp%1000)*1e6);  // 纳秒部分
    msg_odom.pose.pose.position.x = odom.x;
    msg_odom.pose.pose.position.y = odom.y;
    msg_odom.pose.pose.orientation.w = cos(odom.angle*0.5);
    msg_odom.pose.pose.orientation.x = 0;
    msg_odom.pose.pose.orientation.y = 0;
    msg_odom.pose.pose.orientation.z = sin(odom.angle*0.5);
    msg_odom.twist.twist.linear.x = odom.linear_speed;
    msg_odom.twist.twist.angular.z = odom.angular_speed;
    // 发布里程计，把数据发出去
    if(rcl_publish(&pub_odom,&msg_odom,NULL)!=RCL_RET_OK)
    {
        Serial.println("error: odom pub failed!");
    }
}


void twist_callback(const void * msg_in)
{
    // 将受到的消息指针转换成 geometry_msgs__msg__Twist 类型的指针
    const geometry_msgs__msg__Twist* msg = (const geometry_msgs__msg__Twist*)msg_in;
    target_linear_speed = msg->linear.x * 1000; 
    target_angular_speed = msg->angular.z; 
    kinematics.kinematics_inverse(target_linear_speed, target_angular_speed, &out_left_speed, &out_right_speed);
    Serial.printf("OUT:left_speed=%f,right_speed=%f\n", out_left_speed, out_right_speed);
    pid_controller[0].update_target(out_left_speed);
    pid_controller[1].update_target(out_right_speed);
}

// 单独创建一个任务运行 micro-ROS 相当于一个线程
void microros_task(void *args)
{
    // 1.设置传输协议并延迟一段时间等待设置的完成
    IPAddress agent_ip;
    agent_ip.fromString("192.168.1.103");                                // 设置agent的IP地址
    set_microros_wifi_transports("fishros", "88888888", agent_ip, 8888); // 设置传输协议
    delay(3000);                                                         // 等待2秒,等待WIFI连接
    // 2.初始化内存分配器
    allocator = rcl_get_default_allocator(); // 获取默认的内存分配器
    // 3.初始化支持
    rclc_support_init(&support, 0, NULL, &allocator); // 初始化支持
    // 4.初始化节点
    rclc_node_init_default(&node, "fishbot_motion_control", "", &support); // 初始化节点
    // 5.初始化执行器
    unsigned int num_handles = 2;                                             // 订阅和计时器的回调数量,注意这是一个要改的参数
    rclc_executor_init(&executor, &support.context, num_handles, &allocator); // 初始化执行器
    // 初始化订阅者，并将其添加到执行其中
    rclc_subscription_init_best_effort(&sub_cmd_vel,&node, ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs,msg,Twist),"/cmd_vel");
    rclc_executor_add_subscription(&executor,&sub_cmd_vel,&msg_cmd_vel,&twist_callback,ON_NEW_DATA);
    // 初始化msg
    msg_odom.header.frame_id = micro_ros_string_utilities_set(msg_odom.header.frame_id, "odom");
    msg_odom.child_frame_id = micro_ros_string_utilities_set(msg_odom.child_frame_id, "base_footprint");
    // 初始化发布者和定时器
    rclc_publisher_init_best_effort(&pub_odom,&node,ROSIDL_GET_MSG_TYPE_SUPPORT(nav_msgs,msg,Odometry),"/odom");
    rclc_timer_init_default(&timer,&support,RCL_MS_TO_NS(50),timer_callback);
    rclc_executor_add_timer(&executor,&timer);
    // 时间同步
    while (!rmw_uros_epoch_synchronized())
    {
       rmw_uros_sync_session(1000);
       delay(10);
    }
    // 循环执行器
    rclc_executor_spin(&executor);
}

// v=w*r
// r = v/w = 0.05/0.1 = 0.5 0.02/0.1 = 0.2 m
void setup()
{
    // 初始化串口
    Serial.begin(115200); // 初始化串口通信，设置通信速率为115200
    // 初始化电机驱动器
    motor.attachMotor(0, 22, 23); // 将电机0连接到引脚22和引脚23
    motor.attachMotor(1, 12, 13); // 将电机1连接到引脚12和引脚13
    // 初始化编码器
    encoders[0].init(0, 32, 33); // 初始化第一个编码器，使用GPIO 32和33连接
    encoders[1].init(1, 26, 25); // 初始化第二个编码器，使用GPIO 26和25连接
    // 初始化PID控制器的参数
    pid_controller[0].update_pid(0.625, 0.125, 0.0);
    pid_controller[1].update_pid(0.625, 0.125, 0.0);
    pid_controller[0].out_limit(-100, 100);
    pid_controller[1].out_limit(-100, 100);
    // 初始化运动学参数
    kinematics.set_wheel_distance(175); // mm
    kinematics.set_motor_param(0, 0.105805);
    kinematics.set_motor_param(1, 0.105805);
    // 测试下运动学逆解
    

    // 创建一个任务运行 micro-ROS
    xTaskCreate(microros_task, "microros_task", 10240, NULL, 1, NULL);
}

void loop()
{
    delay(10);                                                                               // 等待10毫秒
    kinematics.update_motor_speed(millis(), encoders[0].getTicks(), encoders[1].getTicks()); // 记得调用更新电机速度函数
    motor.updateMotorSpeed(0, pid_controller[0].update(
                                  kinematics.get_motor_speed(0)));
    motor.updateMotorSpeed(1, pid_controller[1].update(kinematics.get_motor_speed(1)));
    // Serial.printf("speed1=%d,speed2=%d\n",kinematics.get_motor_speed(0),kinematics.get_motor_speed(1));
    // Serial.printf("x,y,yaw=%f,%f,%f\n", kinematics.get_odom().x, kinematics.get_odom().y, kinematics.get_odom().angle);
}
