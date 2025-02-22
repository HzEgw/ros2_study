#ifndef __PID_CONTROLLER_H__
#define __PID_CONTROLLER_H__


class PidController
{
    public: 
        PidController() = default;
        PidController(float kp, float ki, float kd, float dt);
    
    private:
        float target_;
        float kp_;
        float ki_;
        float kd_; 
        // pid
        float error_;
        float error_sum_;
        float derror_;
        float prev_error_;
        float intergral_up_ = 2500; // 积分上限
    
    public:
        float update(float current); // 提供当前值，返回下次输出值，也就是PID的结果


}


#endif // __PID_CONTROLLER_H__