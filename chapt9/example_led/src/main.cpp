#include <Arduino.h>
void setup()
{
    pinMode(2, OUTPUT); // 设置2号引脚模式为OUTPUT模式
}
void loop()
{
    digitalWrite(2, LOW); // 低电平，打开LED灯
    delay(100);
    // 休眠1000ms
    digitalWrite(2, HIGH); // 高电平，关闭LED灯
    delay(100);
}