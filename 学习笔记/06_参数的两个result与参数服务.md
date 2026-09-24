# 06 · 参数的那两个 `result`，与"参数其实走服务"

> **记录时间**：2026-09-24（夜）
> **① 问题来源（我自己写的原始笔记）**：`w1_study_workspace/src/w1_param_demo/src/READEME.MD`
> **② 答案与证据（AI 整理，含源码行号 + 我今晚的实验）**
> **③ 认领**：§9 留空 —— 能脱稿讲出"三问三答"才算我的

---

## 1. 一句话结论（三问三答）

| 我的问题 | 一句话答 |
|---|---|
| **① `get_parameter` 源码里的 `bool result` 我没给它赋值，为什么还能用？难道 bool 默认是 1？** | **那行是"初始化"，不是"未赋值"** —— `bool result = get_parameter(sub_name, parameter_variant);` 右边先执行。`result` 的内容 = **内层调用返回的"读成功了吗"**，跟默认值无关 |
| **② `SetParametersResult` 里的 `successful` 有什么用？** | **写操作的裁决**：`false` = **拒绝**（参数值不变 + 客户端收到失败），`true` = 接受。⚠️ **默认构造就是 `false`**（不赋值 = 默认拒绝） |
| **③ 这两个 `result` 怎么关联？参数回调是不是服务回调？** | **两个 `result` 没有任何调用关系**（同名纯属巧合：一个管"读"、一个管"写"）；**但"参数走服务"你说对了** —— `ros2 param set` = 调用节点的 `/set_parameters` **服务**，服务处理器里再调**你注册的回调** |

---

## 2. 我的原始问题（原样保留，含错别字）

> 问题1：这个源码 `get_parameter(sub_name, parameter_variant);` 的 `result` 我第一次的时候还没有赋值，并且 `return result;` 返回数值的时候也没有说 `result` 是 0 还是 1，难道 bool 类型的默认是 1？
> ……既然我第一次还没有给 `result` 赋值，那么 `parameter = static_cast<ParameterT>(...)` 这一步没办法完成，但是 `this->get_parameter("robot_name",robot_name_);` 这个为什么又能够起到我想要的效果呢
> （实验）`ros2 param get /param_node max_speed` → `Double value is: 2.0` …… 我尝试调用，发现确实第一次修改是起作用的，那么这是为什么呢
>
> 问题2：……这段代码的 `result` 有什么作用，我尝试把 `result` 改成 0 尝试一下（截图）
> ！那好似不是说我的知识点应该可以串起来了，我的问题1和问题2或许从来就不是分割的一部分，或许我问题2加入的回调函数就是给回调函数中会用到的？
> 意思也就是说，我写的参数回调函数实际上我的服务的 `set_param` 的回调函数中的回调函数，类似于嵌套函数
> 但是 `add_on_set_parameters_callback` 的 `result` 和 `get_parameter` 的 `result` 怎么关联起来呢？

---

## 3. 为什么会这样（原理 + 源码）

### 3.1 ① `get_parameter` 的 `result` = "读成功了吗"

```cpp
// rclcpp/node.hpp —— Node::get_parameter 的实现
Node::get_parameter(const std::string & name, ParameterT & parameter) const
{
  std::string sub_name = extend_name_with_sub_namespace(name, this->get_sub_namespace());
  rclcpp::Parameter parameter_variant;
  bool result = get_parameter(sub_name, parameter_variant);   // ← 这是【初始化】：右边先算
  if (result) {
    parameter = static_cast<ParameterT>(parameter_variant.get_value<ParameterT>());
  }
  return result;                                              // ← 把"成功了吗"往上抛
}
```
- **我的误读点**：把 `bool result = f(...)` 当成"先声明一个没赋值的 bool" → 于是问"bool 默认是 1？"
  → **这里根本没用到默认值**：`result` 一出生就是**函数返回值**。
- 为什么 `this->get_parameter("robot_name", robot_name_)` 有效：
  → 前面 **`declare_parameter("robot_name","fishbot")` 已创建参数并写入默认值** → 参数**存在** → 返回 `true` → `if (result)` 成立 → 值写进 `robot_name_` ✅
- **反例（5 秒可验）**：读一个**没 declare** 的参数 → 返回 `false` → **`robot_name_` 保持原值、不会崩** —— 这就是这个 `result` 的全部用途。
- 我的实验（默认值改成 `2.0` / `3000` → `ros2 param get` 读回 `2.0` / `3000`）✅ 正好证明 **declare 的默认值生效**。

### 3.2 ② `SetParametersResult.successful` = "写允许吗"

```cpp
// rcl_interfaces/msg/detail/set_parameters_result__struct.hpp
42:      this->successful = false;      // ← 默认构造就设成 false
61:  _successful_type successful;
```
→ **不显式写 `true`，默认就是"拒绝"**。
→ 我的实验（设成 `false`）**正是这个事实的实证**：GUI 面板显示 `3.0`，而 `ros2 param get` 读回 **`2.0`** —— **设置被否决** ✅

### 3.3 ③ 两个 `result` 无关，但**参数确实走服务**（你说对的那半）

**源码证据**（`rclcpp/parameter_service.hpp`）：每个节点自带这些**标准服务**：
```
GetParameters / GetParameterTypes / SetParameters / SetParametersAtomically
DescribeParameters / ListParameters
```
**完整链条**：
```
ros2 param set /param_node max_speed 3.0
   → 调用服务 /param_node/set_parameters          （rclcpp::ParameterService 提供）
      → 服务处理器
         → 【你注册的回调】add_on_set_parameters_callback   ← 裁决 + 同步成员变量
            → 返回 SetParametersResult.successful
      → 服务返回 results[]
   → 客户端打印 success / failure
```
- 所以**不是"函数嵌套"**，而是"**服务处理器 → 你的回调**"；你的直觉方向是对的 ✅
- **两个 `result` 正好站在这条链的两端**：读端是 `bool`（局部变量），写端是 `successful`（消息字段）——**长得像，没有血缘。**

### 3.4 附带一条：回调句柄必须"接住"（与 `07` 篇同一个道理）

```cpp
// rclcpp/node_interfaces/node_parameters.hpp
176:  OnSetParametersCallbackHandle::SharedPtr add_on_set_parameters_callback(...)
187:  using CallbacksContainerType = std::list<OnSetParametersCallbackHandle::WeakPtr>;   // ← 弱引用！
```
→ 节点内部只存 **`WeakPtr`** → **返回的 `SharedPtr` 不接住 = 回调当场失效**（我今天做对了：存进了 `param_onback`）。

---

## 4. 验证（命令 + 今晚已有的证据）

```bash
ros2 service list | grep param               # ① 参数确实有服务：/param_node/set_parameters 等
ros2 param list /param_node                  # ② 三个参数都在
ros2 param set /param_node max_speed 3.0     # ③ 拒绝实验（证据 = 今晚的截图：GUI 3.0 / get 读回 2.0）
```

---

## 5. 要不要 / 什么时候用（判断标准）

| 场景 | 需要参数回调吗 |
|---|---|
| 参数只在**启动时读一次**（如串口号） | ❌ 不需要 —— `uart_bridge` 就是这种（`port` 只在构造时读 → **改端口必须重启节点**） |
| 参数要**运行时联动**（周期变了重建定时器） | ✅ 需要（今晚这个例子） |
| 要**校验/拒绝**非法值（速度上限、周期范围） | ✅ 需要（`successful=false`，并可填 `reason`） |
| 参数改了要**同步到成员/硬件**（波特率、PID 系数） | ✅ 需要 |

---

## 6. 正确 ✗ 错误 对照（对着我今晚的代码）

| ✗ | ✅ | 说明 |
|---|---|---|
| 以为 `bool result = f(...)` 是"未赋值" | 它是**初始化** | §3.1 |
| 以为 `SetParametersResult` 默认"允许" | **默认 `false` = 拒绝** | 源码 `:42` |
| `report_period_ms_ = report_period_ms_;`（自赋值） | `= para.as_int();` | 详见 `学习C_C++语言补充\13` |
| 实验结论只留截图 | 写一行**文字结论**（GUI 3.0 / get 2.0 = 被拒） | 方便检索 |
| 回调句柄用完就丢 | **存成成员**（内部是 `WeakPtr`） | 同 `07` 篇 |

---

## 7. 口诀 & 排查清单

> **口诀：读的 `result` 管"读到没有"，写的 `successful` 管"让不让改"；两个同名、毫无血缘；参数设置走的是 `/set_parameters` 服务。**

| # | 现象 | 先查 |
|:---:|---|---|
| 1 | `ros2 param set` 报失败 | 回调是不是返回了 `successful=false`（或**没写** → 默认 false） |
| 2 | `ros2 param get` 的值没变 | 同上（被拒）／或回调里**没把值同步给成员** |
| 3 | 参数改了但程序行为不变 | 成员没同步（自赋值/漏赋值）／或该参数只在启动时读一次 |
| 4 | 想看参数怎么进来的 | `ros2 service list \| grep param` → `/set_parameters` |

---

## 8. 关联知识

| 项 | 内容 |
|---|---|
| 源码 | `rclcpp/node.hpp`（`get_parameter`）、`rclcpp/parameter_service.hpp`（6~7 个服务）、`rcl_interfaces/.../set_parameters_result__struct.hpp:42`、`rclcpp/node_interfaces/node_parameters.hpp:176/187` |
| 我的代码 | `w1_param_demo/src/para_demo.cpp`（22:36 修正版，`0 warning`） |
| 相关笔记 | **`学习C_C++语言补充\13`（自赋值 / 编译器不报）**、本夹 `04`（服务：一问一答 / 回调形态）、`05`（colcon/source/ext4） |
| 计划接口 | `uart_bridge` 的 `port` 只在启动时生效 → **W2 动手项：给它加参数回调 + 改完端口重新 `openPort()`** |

---

## 9. 一句话复述（**留空 —— 自己写**）

> ① 读的 `result` 管什么：______________________________________
>
> ② 写的 `successful` 管什么：__________________________________
>
> ③ 参数是怎么走到我的回调里的：______________________________

---

*笔记结束 ｜ 2026-09-24 ｜ 问题由我出，证据与整理由 AI 补*

