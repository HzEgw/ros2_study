# 05 · 三问：`colcon build` 在干什么 / 为什么要 `source` / 为什么必须 ext4

> **记录时间**：2026-09-24
> **① 问题来源（我自己写的）**：`学习笔记/ROS2命令/notes` §5 —— 我在命令笔记本里**自己答的**三问
> **② 校对与补充（AI 整理）**：逐条判"✅ 你说对的 / ➕ 要补的"，并给**可验证判据**
> **③ 认领**：§9 留空 —— 能脱稿讲出才算我的

---

## 1. 一句话结论（三问三答）

| 问题 | 一句话答 |
|---|---|
| **`colcon build` 在干什么？** | 它是**构建调度器**（自己不做编译）：按依赖顺序对每个包跑 **CMake → make**；产物分三处：`build/`（中间物）、**`install/`（成品 + 环境钩子）**、`log/`（报错先看它）；自定义 msg/srv 还会由 **`rosidl` 生成 C++ 头文件**。⚠️ **不烧写**（对比 Keil 的"编译→链接→下载"） |
| **为什么要 `source install/setup.bash`？** | 环境变量是**每个 shell 自己的**。它把 `install/<包>/share/<包>/hook/*.sh` 跑一遍 → 设置 **`AMENT_PREFIX_PATH` / `PATH` / `LD_LIBRARY_PATH` / `PYTHONPATH`** → `ros2 run` 才找得到"包"和"可执行文件" |
| **为什么必须 ext4、不能 `/mnt/d`？** | NTFS 上**可执行位不保留、符号链接支持不完整、大小写不敏感** —— 而 colcon/CMake 这三样全都要用 → 会以"脚本没权限 / 软链失败 / 文件找不到"的形式炸掉 |

---

## 2. 我的原始答案（**原样保留**，含我的错别字 —— 留着思考痕迹）

> ① colcon build 在干什么？
> 构建包，我只知道我写的自定义消息接口，ros2会在build之后帮我生成头文件，souce的路径也是在install下面的，所以；build不能少
>
> ② 为什么要 source install/setup.bash？
> 类似是添加路径，实际上ros2 run 我更加偏向于是自动帮我找到我要启动的程序，那么相应的年就要提前加入途径
>
> ③ 为什么必须在 ext4、不能 /mnt/d？
> d盘那就编译不了阿

---

## 3. 逐条校对

### ① `colcon build`

- ✅ **你说对的**：**"会帮我生成头文件"**（自定义 msg/srv 正是靠 `rosidl` 生成）＋**"source 的路径在 install 下面"**＋**"build 不能少"** —— 方向与关键点都对。
- ➕ **要补的**：
  1. **`colcon` 本身不编译**，它是"编排者"：读 `package.xml` 的依赖顺序，对每个包**调 CMake 配置 → make 编译/链接**；
  2. **三处产物**：`build/`（`.o`、`.so`、CMake 缓存）· **`install/`**（`lib/<包>/可执行文件` + `share/<包>/` + **`setup.bash`**）· `log/`（**构建报错先翻这里**）；
  3. **对比 Keil**：Keil = 编译 + 链接 → `.axf` → **烧写**；colcon = 配置 + 编译 + 链接 → **装进 `install/`** → **不烧写**（目标机是 PC）；
  4. `--symlink-install` 只让 **launch/配置等资源文件**用软链（改了不用重编）；**C++ 源码改了仍要重新 `colcon build`**（增量只要几秒）。

### ② `source install/setup.bash`

- ✅ **你说对的**：**"添加路径"**＋**"让 `ros2 run` 自动找到要启动的程序"** —— 意思完全正确。
- ➕ **要补的（具体是哪几个变量）**：
  - **`AMENT_PREFIX_PATH`**（有哪些工作空间/包 → `ros2 run`、`ros2 pkg` 靠它**找包**）
  - **`PATH`**（可执行文件在哪）· **`LD_LIBRARY_PATH`**（`.so` 在哪）· `PYTHONPATH`（Python 包）
  - **可验证**：`ros2 pkg prefix uart_bridge` → 应打印 `.../install/uart_bridge`（**不 source 会报错**）
  - ⚠️ **覆盖顺序**：后 `source` 的排在前面（overlay）→ **同名包会被盖住**（见本夹 `02` 篇）
  - ⚠️ **每个新终端都要 source**（环境变量不跨 shell 继承）

### ③ ext4 vs `/mnt/d`

- ✅ 方向对（**"那就编译不了"**）
- ➕ **要补的具体三条**（这才是"能脱稿讲"的版本）：
  1. **可执行位**：NTFS 挂载后不保留 Linux 的 `+x` → 生成的脚本报 `Permission denied` / `bad interpreter`
  2. **符号链接**：`install/` 里大量软链 → NTFS 支持不完整 → 直接失败
  3. **大小写敏感 + 性能**：Linux 里 `Foo.h` ≠ `foo.h`，NTFS 里是同一个；且经 FUSE 访问慢很多
- **判据**：`df -T ~/ros2_ws` → 应显示 **ext4**；`mount | grep media` 若是 `ntfs3`/`fuseblk` 就别在那儿 build

---

## 4. 自己动手验证（3 条命令，1 分钟）

```bash
df -T ~/ros2_ws                    # 应显示 ext4 ✅
ros2 pkg prefix uart_bridge        # 未 source 时会报错；source 后打印 install/uart_bridge
echo $AMENT_PREFIX_PATH            # source 前 / 后对比（会多出 ~/ros2_ws/install/...）
```

---

## 5. 什么时候用得上（判断标准）

| 现象 | 先想哪一条 |
|---|---|
| 新终端里"找不到包 / 找不到可执行" | **②**（忘了 source） |
| 改完代码行为没变 | **①**（要不要重编）＋ source **顺序** |
| 想"干脆把工作空间放 D 盘省事" | **③**（别） |
| 构建报错一堆 | **①** 里的 `log/`（`colcon` 的报错日志） |

---

## 6. 正确 ✗ 错误 对照

| ✗ | ✅ |
|---|---|
| 以为 `colcon` 就是编译器 | `colcon` 是**调度器**；真正编译的是 **CMake/make** |
| 以为 `source` 只是"配个路径" | 它设**一整套**环境变量 + 包索引，还有**覆盖顺序** |
| 在 `/mnt/d` 上 `colcon build` | 只在 **ext4**（`~/ros2_ws`） |
| 以为 `--symlink-install` 让 C++ 改完不用编 | 它只对**资源文件**有效；C++ **仍要重编** |

---

## 7. 口诀

> **colcon 是调度器（`build/` 中间物 · `install/` 成品 · `log/` 日志）；`source` 是"把这个终端接进这个工作空间"；NTFS 上别 build（权限、软链、大小写三件事全不满足）。**

---

## 8. 关联知识

| 项 | 内容 |
|---|---|
| 我的原始答案 | `学习笔记/ROS2命令/notes` §5（原样保留在上文 §2） |
| 相关笔记 | `02`（同名包 / overlay 与 source 顺序）、`01`（计数）、`03`（publish 实体在哪）、`04`（服务三问） |
| 计划出处 | `03` §4（"每个会话留下能复现的命令"）、`04` §3（编译步骤）与 §5（报错表） |
| 可验证命令 | `df -T`、`ros2 pkg prefix`、`echo $AMENT_PREFIX_PATH`、`ls ~/ros2_ws/{build,install,log}` |

---

## 9. 我的复述（**留空 —— 自己写；能脱稿讲出"三问三答"才算我的**）

> ① `colcon build` 在干什么：______________________________
>
> ② 为什么要 `source`：______________________________________
>
> ③ 为什么必须 ext4：______________________________________

---

*笔记结束 ｜ 2026-09-24 ｜ 问题与初答由我写，校对由 AI 补*
