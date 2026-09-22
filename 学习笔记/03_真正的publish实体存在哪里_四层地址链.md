# 03 · 真正的 publish 实体存在哪里？—— 从 `rclcpp` 到 DDS 的四层地址链

> **记录时间**：2026-09-22
> **① 问题来源（我自己写的原始笔记）**：`~/ros2_study_myself/w1_study_workspace/src/w1_service_demo/src/READEME.md`
> **② 答案与证据（AI 整理，源码/实测可查）**：见各节末尾的"证据"行
> **③ 认领方式**（见 `../README.md` §2）：读完后**用自己的话复述**一遍，能脱稿讲清才算"我的"

---

## 1. 一句话结论

```cpp
rclcpp::Publisher<StringDate>::SharedPtr pub_;   // 你手上这个
```
**它只是最外层的 C++ 包装**；真正的 publish 实体经过 **4 层，每层各占自己的内存**：

| 层 | 是什么 | 在哪 | 证据 |
|---|---|---|---|
| **①** | `rclcpp::Publisher<String>` | **堆**（`create_publisher` 里 `make_shared<PublisherT>` 分配）| 09-20 实测：本体离 node ≈ **2.4 MB**；`pub_.get()` 就是它 |
| **②** | **`rcl_publisher_t`（C 语言结构体）** | **另一块独立堆内存**（**没有**内嵌在 ① 里！） | `publisher_base.hpp:337` → `std::shared_ptr<rcl_publisher_t> publisher_handle_;` |
| **③** | **DDS 实体（Fast DDS 的 `DataWriter`）** | **DDS 库自己的内存** | 本机装的是 `<b>librmw_fastrtps_cpp.so</b>` + `libfastrtps.so.2.6.12` |
| **④** | **消息数据本身（samples）** | **DDS 的样本池 / 序列化缓冲** | ✅ **我猜的"dds 的缓冲区"方向正确** |

> 一句话：**包装在堆，句柄也是堆，实体进 DDS，数据进缓冲。**

---

## 2. 我的原始疑问（原样保留，便于以后搜索）

> "我的节点对象……所有的对象内部确实内存都存在于 new 区，这是为了我的节点对象的生死让我自己决定。
> 是我对象内部保存的只是我的 publish 的句柄。**那么真正的 publish 保存在哪里呢**？
> 首先 DDS 传输是要依靠 C 语言编写的，这就意味着我的 DDS 传输通道的"类"publish 应该都是结构体，不是 C++ 的类。
> 那我写的智能指针的目的就是为了接管真实的 publish 结构体实例化的内存。
> **那么我的 publish 实例化到底存在哪里？** 这或许涉及到某一个高级知识，
> 但是我猜测 ros2 这个工程化程度极高的肯定是设置了一个专门的内存区域来统一放置这些内存。
> 至于之前说的那个特别的内存区域，**其实应该就是 DDS 的缓冲区**。"

**判定**：你的推理链（*C 结构体 → 智能指针接管 → DDS 区域*）**基本成立**，只有"专门区域"这个措辞要换掉 —— 不是"一个专区"，而是**每层各自的堆对象 + DDS 自己管理的池**。

---

## 3. 为什么会这样（逐层原理）

### 3.1 ① →②：C++ 包装里放的是"一个 C 句柄的 `shared_ptr`"

```cpp
// /opt/ros/humble/include/rclcpp/rclcpp/publisher_base.hpp:337
std::shared_ptr<rcl_publisher_t> publisher_handle_;
```
- `rcl_publisher_t` **本体不在** `rclcpp::Publisher` 对象内部（这点 09-20 的 `sizeof` 也印证了：`sizeof(pub_)=16`，只有两个指针）；
- 它是被 **`shared_ptr` 接管**的，`shared_ptr` 的 deleter 会调用 **`rcl_publisher_fini()`** →
  这正好解释 09-20 的实测现象：**不接住 `pub_` → `shared_ptr` 析构 → `rcl_publisher_fini` → 话题从 ROS 图消失**。
- 👉 你说的"**智能指针的目的是接管真实的 publish 结构体**"——**源码级成立**。

### 3.2 ② →③：`rcl` → `rmw` → DDS

```
rclcpp::Publisher (C++ 包装)
   └─ rcl_publisher_t        （rcl：C 中间层，统一 API）
        └─ rmw_publisher_t   （rmw：中间件抽象层）
             └─ eprosima::fastdds::dds::DataWriter  ← 真正干活的实体（在 Fast DDS 里）
```
- **证据（本机）**：`/opt/ros/humble/lib/librmw_fastrtps_cpp.so`、`/opt/ros/humble/lib/libfastrtps.so.2.6.12`（**Fast DDS 2.6.12**，8.1 MB）→ 你的 ROS2 用的是 **eProsima Fast DDS**。

### 3.3 ③ →④：你写的 `msg` 只是"内容来源"，DDS 自己有一份存储

- 你 `publish(msg)` 时，`msg` 是**你栈上/堆上的对象**；DDS 会把它**序列化进自己的样本/历史缓冲**，
  再由 Fast DDS 的 **DataWriter** 按 QoS 发送（这也是 `ros2 topic hz`、`ros2 bag` 能"另存一份"的原因）。
- 所以**你猜的"DDS 缓冲区"是对的**，只是要分清：**实体（③）在 DDS 对象里，数据（④）在 DDS 缓冲里**。

---

## 4. 最小复现（自己动手把这四层抓出来）

```bash
# ① 看到第 ② 层：C 句柄是被 shared_ptr 接管的（不是内嵌对象）
grep -n 'rcl_publisher_t' /opt/ros/humble/include/rclcpp/rclcpp/publisher_base.hpp

# ② 看本机用的是哪家 DDS（第 ③ 层）
ls /opt/ros/humble/lib/ | grep -E '^librmw_|libfastrtps'
#   期望看到：librmw_fastrtps_cpp.so / libfastrtps.so.2.6.12

# 或者直接问 ROS2（2026-09-22 实测输出，原文照抄）：
ros2 doctor --report | grep -A2 'RMW MIDDLEWARE'
#     RMW MIDDLEWARE
#   middleware name    : rmw_fastrtps_cpp

# ③ 看第 ① 层的地址（在节点构造函数里）
std::printf("&pub_ = %p   pub_.get() = %p   sizeof(pub_) = %zu\n",
            (void*)&pub_, (void*)pub_.get(), sizeof(pub_));
#   09-20 实测：&pub_ 偏移 896（在 node 内部）、本体离 node ≈2.4 MB、sizeof=16
```

**观察到的三件事就是证据**：`sizeof(pub_)=16`（①只有两个指针）→ ②在别处（源码里是 `shared_ptr` 成员）→ ③在 DDS 库里（`libfastrtps` 存在）。

---

## 5. 要不要 / 什么时候用（判断标准）

| 什么时候必须关心这几层 | 为什么 |
|---|---|
| **性能**（延迟 / 吞吐 / 零拷贝） | 数据要跨 ③④ 边界（序列化 + DDS 缓冲拷贝）——这就是 micro-ROS 与"串口裸传"的对比维度 |
| **内存吃紧**（嵌入式 / MCU） | DDS 缓冲是**额外的**内存开销（F103 上 20KB RAM 跑不动 DDS 就是这个原因） |
| **调试"话题看不见 / 收不到"** | 问题可能在 ②③（DDS 发现/图），而不是你的 C++ 代码 |
| **写 micro-ROS（F407 阶段）** | micro-ROS 也是"另一个中间件实现"，**同一套分层**，只是把 DDS 换成了 XRCE-DDS agent |

---

## 6. 正确 ✗ 错误 对照

| ✗ 常见误解 | ✅ 正确 |
|---|---|
| "`pub_` 就是 DDS 的实体" | 它只是**最外层 C++ 包装**；实体在 DDS 库里（第 ③ 层） |
| "`rcl_publisher_t` 内嵌在 `rclcpp::Publisher` 里" | 源码：它是 **`shared_ptr<rcl_publisher_t>` 成员**，本体在另一块堆内存 |
| "我 `publish` 的那个 `msg` 就是 DDS 里的数据" | DDS 会**托管一份自己的样本/缓冲**（你那份只是来源） |
| "ROS2 设了一块专用内存区放这些" | 不是"专区"，而是**每层各自的堆对象** + DDS 自己的池 |
| "句柄所有权是节点管的" | 节点只**登记**（`add_publisher`），**所有权在你手里**（见 D 盘 `07`/`10` 篇） |

---

## 7. 口诀 & 排查清单

> **口诀：包装在堆，句柄也是堆，实体进 DDS，数据进缓冲。**

| # | 想知道 | 怎么查 |
|:---:|---|---|
| 1 | 最外层对象在哪、多大？ | `&pub_` / `pub_.get()` / `sizeof(pub_)` |
| 2 | C 句柄是谁管的？ | `grep rcl_publisher_t …/publisher_base.hpp`（看是不是 `shared_ptr`） |
| 3 | 用的是哪家 DDS？ | `ls /opt/ros/humble/lib \| grep -E 'rmw\|fastrtps'`；`ros2 doctor --report` |
| 4 | 话题在不在图上？ | `ros2 topic info -v`（图 = ③ 层的可见性） |
| 5 | 内存被谁吃了？ | 先想到 ④（DDS 缓冲 + history 深度 QoS） |

---

## 8. 关联知识

| 项 | 内容 |
|---|---|
| 源码 | `publisher_base.hpp:337`（句柄）、`create_publisher.hpp:46`（工厂）、`publisher_factory.hpp:76`（`make_shared<PublisherT>`） |
| 本机中间件 | `librmw_fastrtps_cpp.so` + `libfastrtps.so.2.6.12`（**Fast DDS**） |
| 相关笔记 | 本夹 `01`（计数）、`02`（同名话题）、**`04`（服务侧的同类问题）**；D 盘 `学习C_C++语言补充\07_句柄与智能指针生命周期.md`、`10_对象包含关系与方法里new出来的内存归谁.md` |
| 下一步 | 服务/客户端是**同一套分层**（`rcl_service_t` → `rmw_service_t` → DDS `Replier`），且服务在 DDS 层其实是**两条 topic** → 见 `04` |

---

*笔记结束 ｜ 2026-09-22 ｜ 问题由我出，证据与整理由 AI 补*

