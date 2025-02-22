#include <Arduino.h>

#define TRIG 27
#define ECHO 21

void setup()
{
    Serial.begin(115200); // 初始化串口
    pinMode(2, OUTPUT);    // 设置2号引脚模式为OUTPUT模式
    pinMode(TRIG, OUTPUT); // 设置TRIG引脚为输出模式
    pinMode(ECHO, INPUT);  // 设置ECHO引脚为输入模式
}
void loop()
{
    // 啊一下，产生一个10微秒的高电平
    digitalWrite(TRIG, HIGH);
    delayMicroseconds(10); // 延时 10 微秒
    digitalWrite(TRIG, LOW);
    // 读取ECHO引脚的高电平持续时间
    double delta_time = pulseIn(ECHO, HIGH);            // 检测高电平持续时间，注意返回值是微秒us
    float detect_distance = delta_time * 0.0343 / 2;    // 计算距离单位 cm, 声速 0.0343
    Serial.printf("distance=%f cm\n", detect_distance); // 打印距离
    delay(500);                                        // 延时 1 秒
}