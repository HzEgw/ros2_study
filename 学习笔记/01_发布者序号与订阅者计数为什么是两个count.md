# 01 · 发布者序号 vs 订阅者计数 —— 为什么两边都要 `count`？

> **记录时间**：2026-09-20
> **触发场景**：写 W1 手写练习（`w1_study_workspace`）时问："我在发布者里已经给消息 `count` 过了，
> 为什么你的订阅者代码里还要再 `count` 一次？"
> **分类判定**：把 ROS2 拿掉，"话题收发两侧各自计数"这个场景**不成立** → **ROS2 问题** → 归这里 ✅
> **对应代码**：`~/ros2_study_myself/w1_study_workspace/src/w1_topic_demo/src/`
> （`topic_publisher_demo.cpp` / `w1_subscriber_demo.cpp`）
> 与参考示例 `~/ros2_study_with_cline/src/w1_topic_demo/src/`（`topic_publisher_node.cpp` / `topic_subscriber_node.cpp`）

---

## 1. 一句话结论

**那不是"同一个计数被数了两次"，而是两个不同进程里、两个互不相干的变量：**

| | 发布者里的 `count` | 订阅者里的 `count` |
|---|---|---|
| 它是什么 | **要被发出去的数据的一部分**（写进 `msg.data`） | **本进程内的统计**（不随消息走） |
| 谁知道它 | 订阅者、`ros2 topic echo`、`ros2 bag` 都能看到 | 只有订阅者自己 |
| 回答的问题 | "这是**发出方**的第几条？" | "这是我**收到**的第几条？" |

**两个都留 = 一眼看出"丢没丢"**：如果订阅者打印"收到第 5 条"、而 payload 里写的是"第 8 条"，
说明中间**丢了 3 条**（或**有第二个发布者**、或订阅者**启动晚了**）。

> 一句话：**本地状态 ≠ 传输数据**；一个进程永远不知道另一个进程数到几，除非通过消息告诉它。

---

## 2. 现象（实测原文，原样保留）

我实测把"参考示例 pub/sub"和"我自己的 pub"同时跑起来后，订阅者打印：

```
[INFO] [w1_topic_subscriber]: 订阅者已启动：正在监听 /w1/chatter
[INFO] [w1_topic_subscriber]: 收到第 1 条：现在已经发送消息第2条      ← 这条是【我的 pub】发的！
[INFO] [w1_topic_subscriber]: 收到第 2 条：Hello ROS2 from W1! 第 3 条
[INFO] [w1_topic_subscriber]: 收到第 3 条：现在已经发送消息第3条      ← 又是我的 pub
[INFO] [w1_topic_subscriber]: 收到第 4 条：Hello ROS2 from W1! 第 4 条
```

**订阅者的"收到第 N 条" 和 payload 里的"第 M 条"不对齐** —— 这正是两个计数器各自独立的最好证明：
一个在数"我收到几条"，另一个在说"这条是发出方的第几条"。

（顺带暴露的第二个问题：**同名话题会把两个发布者混到一起** → 见本文件夹 `02` 篇。）

---

## 3. 为什么会这样（原理）

1. **两个节点是两个进程**：各自有独立的地址空间、独立的变量。发布者的 `count_` 和订阅者的 `count_`
   可能在源码里**同名**，但那是两个对象、两块内存（同名只是命名习惯）。
2. **能跨进程传递的只有"消息"**：想让对方知道"现在几条了"，**必须把它写进 payload**（所以我在 pub 里
   把序号拼进字符串）。**本地变量不会自己过去。**
3. **对照才有诊断价值**：
   - 只用 payload 里的序号 → 你看到的是"发出方认为的号"，**丢包完全不可见**；
   - 只用本地计数 → 只知道"收了几条"，不知道"该收几条"；
   - **两个一起看 → 差值 = 丢了/混进来的条数**。
4. 这也是以后做 **micro-ROS 对比实验**要用的方法：MCU 端序号 vs ROS2 端收到计数 →
   直接量化"串口/WiFi 链路的丢包率"（`00` 计划 §9 的对比实验表格里就有这一栏）。

---

## 4. 最小复现（W1 手写那套就是最小复现）

**发布者关键两行**（`topic_publisher_demo.cpp`）：
```cpp
msg.data = "现在已经发送消息第" + std::to_string(count++) + "条";   // 序号写进消息（后置++ → 第一条是 0）
pub_->publish(msg);
```

**订阅者关键一行**（`w1_subscriber_demo.cpp`，我只打印内容）：
```cpp
RCLCPP_INFO(this->get_logger(), "%s", msg.data.c_str());
```

**参考示例的订阅者**（多了一个本地计数，用于对照）：
```cpp
RCLCPP_INFO(this->get_logger(), "收到第 %lu 条：%s",
            static_cast<unsigned long>(++count_), message->data.c_str());
```

```bash
source /opt/ros/humble/setup.bash
ros2 run w1_topic_demo topic_publisher_node      # 终端 1
ros2 run w1_topic_demo topic_subscriber_node     # 终端 2
ros2 topic echo /w1/chatter                      # 终端 3：只看 payload 里的序号
ros2 topic hz   /w1/chatter                      # 看频率是否稳定（丢了会掉）
ros2 topic info /w1/chatter -v                   # 看有几个发布者/订阅者（本文件夹 02 篇要用）
```

---

## 5. 要不要 / 什么时候用（判断标准）

| 问题 | 建议 |
|---|---|
| payload 里要不要放序号？ | **要**（调试期一定放）：它是"丢包/乱序"的唯一线索；正式发布时可省带宽（但建议至少留一个可选的 debug 字段） |
| 订阅者要不要自己计数？ | **要**（调试期）：否则永远看不出丢包。**要**（长期）：做统计/限流/首条日志也需要 |
| 只想看内容 | 那就只打印 `msg.data`（我现在这样）—— 但**一旦出现"少了几条"就查不出来** |
| 什么时候可以不计数 | 单机 demo、流量很小、只关心"最新值"（如传感器最新温度）时 |

---

## 6. 正确 ✗ 错误 对照

| ✗ 错误认知 | ✅ 正确 |
|---|---|
| "pub 和 sub 的 `count` 是同一个变量被数了两遍" | **两个进程、两个变量**，只是名字像 |
| "payload 里有序号，就不用本地计数了" | 那样**丢包不可见**（看到的永远是发出方写的号） |
| "本地计数能告诉我收全了没有" | 不能 —— 它只知道"收了几条"，不知道"该收几条"；**必须两边对照** |
| `std::to_string(count++)` 期望第一条是 1 | 后置 `++` → **第一条是 0**；要 1 基就用 `++count`（见 `学习C_C++语言补充\06`） |
| 用 `%s` 直接塞 `std::string` | 要用 `msg.data.c_str()`（否则乱码，实测见 `学习C_C++语言补充\` 相关篇） |

---

## 7. 口诀 / 排查清单

> **口诀：payload 里的号是"发出的"，本地计数器是"收到的"，两边一减才算数。**

排查"少收到消息"：

| # | 查什么 | 命令 |
|:---:|---|---|
| 1 | 发布频率稳不稳？ | `ros2 topic hz /话题` |
| 2 | 有几个发布者？ | `ros2 topic info /话题`（Publisher count） |
| 3 | 谁在发？ | `ros2 topic info /话题 -v` |
| 4 | 订阅者是不是启动晚了？ | 对照"第一条收到的 payload 序号" |
| 5 | 队列深度够不够？（发布太快、回调太慢会丢） | 看 `create_publisher(话题, 队列长度)` 的值，试试调大 |
| 6 | 有没有**另一个**同名话题的发布者在捣乱？ | 本文件夹 `02` 篇 |

---

## 8. 关联知识

| 项 | 内容 |
|---|---|
| 我的源码 | `~/ros2_study_myself/w1_study_workspace/src/w1_topic_demo/src/topic_publisher_demo.cpp:14`、`w1_subscriber_demo.cpp:12` |
| 参考示例 | `~/ros2_study_with_cline/src/w1_topic_demo/src/topic_subscriber_node.cpp:42-43` |
| 相关命令 | `ros2 topic hz/echo/info -v`、`ros2 node info <节点>` |
| 相关笔记 | 本文件夹 `02_同名包与同名话题为什么会串.md`；`学习C_C++语言补充\06`（`++` 前后缀）、`07`（句柄必须被接住） |
| 计划里的用途 | `00_8个月学习计划.md` §9 对比实验素材（ESP32 官方固件 vs STM32 自研固件 → 链路丢包/延迟对比） |

---

*笔记结束 ｜ 2026-09-20*
