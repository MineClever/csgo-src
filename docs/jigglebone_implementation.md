# JiggleBone 抖动骨骼 — 实现原理文档

> 源码版本：CS:GO Source Engine (refactored leak)
> 原始作者：Michael Booth (Turtle Rock Studios)

---

## 概述

JiggleBone（抖动骨骼）是一套基于弹簧-质点模型的物理模拟系统，用于给角色骨骼添加**次级运动 (secondary motion)**。
使松散连接的部位（耳朵、尾巴、头发、衣物挂件、背包带子等）在角色运动时产生自然的滞后与弹性振荡。

设计思路是**后处理式的**：先有标准骨骼动画结果，然后在后处理环节用物理模拟覆盖。

---

## 文件结构

| 文件 | 层 | 职责 |
|---|---|---|
| `src/public/studio.h` (mstudiojigglebone_t) | 模型数据 | 定义编译进 .mdl 的物理参数 |
| `src/public/jigglebones.h` (CJiggleBones, JiggleData) | 运行时状态 | 核心类声明与每骨骼状态管理 |
| `src/public/jigglebones.cpp` (BuildJiggleTransformations) | 物理仿真 | 弹簧-质点模拟、约束求解、矩阵构造 |
| `src/game/client/c_baseanimating.h/.cpp` | 集成层 | 在客户端骨骼更新循环中调度 JiggleBone |

---

## 模型数据层：mstudiojigglebone_t

定义于 `studio.h:197`。每个开启了 JiggleBone 的骨骼在 .mdl 中包含一个该结构体实例。

### Flags 位掩码

```c
#define JIGGLE_IS_FLEXIBLE           0x01   // 启用弹性仿真
#define JIGGLE_IS_RIGID              0x02   // 启用刚性仿真
#define JIGGLE_HAS_YAW_CONSTRAINT    0x04   // 偏航角铰链约束
#define JIGGLE_HAS_PITCH_CONSTRAINT  0x08   // 俯仰角铰链约束
#define JIGGLE_HAS_ANGLE_CONSTRAINT  0x10   // 最大偏转角（锥形约束）
#define JIGGLE_HAS_LENGTH_CONSTRAINT 0x20   // 锁定骨骼长度
#define JIGGLE_HAS_BASE_SPRING       0x40   // 根部独立弹簧
```

### 参数字段分组

```cpp
struct mstudiojigglebone_t {
    int   flags;
    float length;         // 骨骼长度（根部到末端的距离）
    float tipMass;        // 末端质点质量（也控制重力强度）

    // ---- Flexible mode: 弹簧系数 ----
    float yawStiffness;    // 偏航（左右）弹簧刚度
    float yawDamping;      // 偏航阻尼
    float pitchStiffness;  // 俯仰（上下）弹簧刚度
    float pitchDamping;    // 俯仰阻尼
    float alongStiffness;  // 轴向弹簧刚度（无长度约束时启用）
    float alongDamping;    // 轴向阻尼

    // ---- 角度锥形约束 ----
    float angleLimit;      // 最大偏转角（弧度）

    // ---- 偏航铰链约束 ----
    float minYaw, maxYaw;  // 限位角度（弧度）
    float yawFriction;     // 沿限位面的摩擦
    float yawBounce;       // 碰撞反弹系数

    // ---- 俯仰铰链约束 ----
    float minPitch, maxPitch;
    float pitchFriction;
    float pitchBounce;

    // ---- 根部弹簧 (Base Spring) ----
    float baseMass;
    float baseStiffness, baseDamping;
    float baseMinLeft, baseMaxLeft;
    float baseLeftFriction;
    float baseMinUp, baseMaxUp;
    float baseUpFriction;
    float baseMinForward, baseMaxForward;
    float baseForwardFriction;
};
```


---

## 运行时状态：JiggleData

定义于 `jigglebones.h`。每帧为每个开启 JiggleBone 的骨骼维护一个实例，记录物理状态。

```cpp
struct JiggleData {
    int   bone;           // 骨骼索引
    int   id;             // 调试用唯一 ID
    float lastUpdate;     // 上次更新时间（防帧率暴涨用）

    Vector basePos;       // 根部世界坐标
    Vector baseLastPos;   // 上帧根部位置（用于速度差分）
    Vector baseVel;       // 根部速度
    Vector baseAccel;     // 根部加速度（累加器）

    Vector tipPos;        // 末端世界坐标
    Vector tipVel;        // 末端速度
    Vector tipAccel;      // 末端加速度（累加器）
};
```

### 状态管理

`CJiggleBones` 内部使用 `CUtlLinkedList<JiggleData>` 按骨骼索引索引。`GetJiggleData()` 做懒分配：
如果该骨骼已有状态则返回，否则创建新条目加入链表头部。

```cpp
class CJiggleBones {
public:
    JiggleData *GetJiggleData(int bone, float currenttime,
                              const Vector &initBasePos, const Vector &initTipPos);
    void BuildJiggleTransformations(int boneIndex, float currentime,
                                    const mstudiojigglebone_t *jiggleParams,
                                    const matrix3x4_t &goalMX,
                                    matrix3x4_t &boneMX,
                                    bool coordSystemIsFlipped);

    CUtlLinkedList<JiggleData> m_jiggleBoneState;
};
```


---

## 核心仿真流程：BuildJiggleTransformations

定义于 `jigglebones.cpp:106`。这是整个系统的核心函数，接收：

| 参数 | 含义 |
|------|---------|
| boneIndex | 当前骨骼索引 |
| currenttime | 当前时间 (gpGlobals->curtime) |
| jiggleInfo | 模型参数指针 |
| goalMX | 骨骼的标准动画矩阵（无物理时的目标姿势） |
| boneMX | 输出：物理模拟修正后的最终矩阵 |
| coordSystemIsFlipped | 坐标系手性标志 |

### 完整流程分阶段图解

```
                    +---------------------------+
                    | 提取 goalBase、goalTip    |
                    | 和局部坐标系 (L/U/F)      |
                    +----------+---------------+
                               |
                    +----------v---------------+
                    | (0) 初始化 & 安全检查     |
                    |   - 超时重置 (>0.5s)     |
                    |   - NaN 检测             |
                    |   - 反向保护 (dot<-0.9)  |
                    +----------+---------------+
                               |
                    +----------v---------------+
                    | (1) Tip 弹簧仿真         |
                    |   - 重力: acc.z -= M     |
                    |   - 子步进 (1/120s)      |
                    |   - 误差->局部坐标投影   |
                    |   - 弹簧加速度计算       |
                    |   - 欧拉积分             |
                    +----------+---------------+
                               |
                    +----------v---------------+
                    | (2) 约束处理             |
                    |   - 偏航铰链 (XZ 平面)   |
                    |   - 俯仰铰链 (YZ 平面)   |
                    |   - 锥形角度约束         |
                    |   - 长度锁定             |
                    +----------+---------------+
                               |
                    +----------v---------------+
                    | (3) 构建朝向矩阵         |
                    |   fwd = (tip-base).norm  |
                    |   left = cross(up, fwd)  |
                    |   up = cross(fwd, left)  |
                    +----------+---------------+
                               |
                    +----------v---------------+
                    | (4) Base Spring (根部)   |
                    |   - 独立弹簧系统         |
                    |   - 三轴限位 + 摩擦      |
                    |   - 更新矩阵位置列       |
                    +----------+---------------+
                               |
                    +----------v---------------+
                    | 输出 boneMX -> 渲染     |
                    +---------------------------+
```

### 阶段 0：初始化与数值安全

1. 从 `goalMX` 提取根位置和局部坐标轴
2. 计算目标末端位置：`goalTip = goalBasePos + length * goalForward`
3. 获取/创建 `JiggleData` 状态
4. **超时重置**：如果 `currenttime - lastUpdate > 0.5s`，说明发生了场景切换或长时间暂停，直接重置到目标位置
5. **方向异常保护**：如果模拟方向与目标方向的点积 < -0.9（几乎反向），即时重置

### 阶段 1：Tip 弹簧仿真

仅在 `JIGGLE_IS_FLEXIBLE` 标志启用时执行，这是抖动效果的主要来源。

**子步进积分**（关键设计）：

```cpp
static const float MIN_DELTA_T_PER_ITERATION = 1.0f / 120.0f;
for (float dT = deltaT; dT > 0.0f; dT -= MIN_DELTA_T_PER_ITERATION)
```

将帧间隔拆分为 <= 1/120s 的子步长，以保证弹簧仿真的数值稳定性，不受帧率波动影响。

**弹簧力计算（每子步）**：

```
error = goalTip - tipPos                      // 全局空间误差
localError.x = Dot(goalLeft,  error)          // 偏航方向误差
localError.y = Dot(goalUp,    error)          // 俯仰方向误差
localError.z = Dot(goalForward, error)        // 轴向误差

// 弹簧加速度 = stiffness * error - damping * velocity
yawAccel   = yawStiffness   * localError.x - yawDamping   * localVel.x
pitchAccel = pitchStiffness * localError.y - pitchDamping * localVel.y

tipAccel += yawAccel * goalLeft + pitchAccel * goalUp
```

在有 `JIGGLE_HAS_LENGTH_CONSTRAINT` 时，不施加轴向力（骨骼长度锁定）；否则额外施加沿骨骼方向的弹簧力。

**欧拉积分**：

```
tipVel  += tipAccel * dt
tipPos  += tipVel  * dt
```

子步结束后清零加速度累加器。


### 阶段 2：约束求解

约束按优先级依次应用，数量取决于 flags。

#### 偏航约束 (Yaw Constraint)

```
yawError = atan2(localAlong.x, localAlong.z);  // 局部 XZ 平面角度
if (yawError < minYaw || yawError > maxYaw) {
    // 1. 构建限位平面的旋转矩阵
    // 2. 将 tipPos 投影到限位平面
    // 3. 在限位平面上施加摩擦:
    //    tipAccel -= yawFriction * tangentVelocity
    // 4. 施加碰撞反弹:
    //    tipVel = -yawBounce * normalVelocity + tangentVelocity
}
```

#### 俯仰约束 (Pitch Constraint)

与偏航对称，在局部 YZ 平面处理。

#### 锥形角度约束 (Angle Constraint)

```
angleBetween = acos(Dot(forward, goalForward));
if (angleBetween > angleLimit) {
    maxDist = length * sin(angleLimit);
    tipPos = goalTip - maxDist * delta.normalized();
}
```

#### 长度约束 (Length Constraint)

```
tipPos = basePos + length * forward;
tipVel -= Dot(tipVel, forward) * forward;  // 轴向速度归零
```

### 阶段 3：构建朝向矩阵

用物理模拟后的 tipPos 计算新的骨骼朝向：

```
forward = (tipPos - basePos).Normalize();
left    = CrossProduct(goalUp, forward).Normalize();
up      = CrossProduct(forward, left);
boneMX  = [left | up | forward | basePos];
```

坐标系手性 (coordSystemIsFlipped) 通过调整叉积顺序解决左右手系问题。

### 阶段 4：Base Spring（根部弹簧）

一个与 Tip 弹簧完全独立的系统，控制**骨骼根部的位置**。

**物理模型**：

```
baseAccel.z -= baseMass              // 重力
error = goalBasePos - basePos
baseAccel += baseStiffness * error - baseDamping * baseVel

baseVel += baseAccel * deltaT
basePos += baseVel  * deltaT
```

**三轴限位**：在 Left/Up/Forward 三个方向分别钳位于 [baseMin*, baseMax*]，各轴独立摩擦。

**特殊行为**：如果只有 Base Spring 而没有 Tip 弹簧，骨骼方向保持 goal 不变，仅位置被弹簧修改。

**速度更新**：用位置差分代替累加，避免漂移：

```
baseVel = (basePos - baseLastPos) / deltaT;
baseLastPos = basePos;
```


---

## 集成流程

在 `c_baseanimating.cpp` 的 `C_BaseAnimating::StandardBlendingRules`（骨骼更新循环）中：

```cpp
if (r_jiggle_bones.GetBool() && m_isJiggleBonesEnabled) {
    for (int i = 0; i < numBones; i++) {
        mstudiojigglebone_t *ji = pStudioHdr->pBone(i)->pjigglebone;
        if (ji) {
            if (!m_pJiggleBones)
                m_pJiggleBones = new CJiggleBones;
            m_pJiggleBones->BuildJiggleTransformations(
                i, gpGlobals->curtime, ji,
                goalMX, GetBoneForWrite(i), ShouldFlipModel());
        }
    }
}
```

执行流程：

1. 由 CVar `r_jiggle_bones` 和 `DisableJiggleBones()` 开关控制是否启用
2. `CJiggleBones` 对象懒创建，`DisableJiggleBones()` 时销毁清空状态
3. 遍历所有骨骼，对 `pjigglebone` 非空的骨骼调用仿真
4. **结果直接写入骨骼矩阵**，覆盖标准动画结果

```
标准骨骼 Blend
    |
    v
IK 求解 / 布娃娃物理
    |
    v
JiggleBone 后处理 <--- 逐骨骼覆盖矩阵
    |
    v
蒙皮 & 渲染
```

---

## 重要 CVar

| CVar | 默认值 | 作用 |
|------|--------|------|
| r_jiggle_bones | 1 | 主开关 |
| cl_jiggle_bone_debug | 0 | 调试显示 (=ID 显示特定骨骼, =1 显示所有) |
| cl_jiggle_bone_debug_yaw_constraints | 0 | 显示偏航约束线框 |
| cl_jiggle_bone_debug_pitch_constraints | 0 | 显示俯仰约束线框 |
| cl_jiggle_bone_invert | 0 | 翻转所有物理向量（调试用） |
| cl_jiggle_bone_sanity | 1 | 方向反向保护开关 |


---

## 关键技术决策

| 决策 | 理由 |
|------|------|
| **子步进积分 (120Hz)** | 弹簧系统在低帧率下会发散，固定子步长保证数值稳定性 |
| **超时重置 (0.5s)** | 防止切场景/暂停/断点后的爆炸式弹跳 |
| **局部坐标系弹簧** | 弹性分解为 yaw/pitch/along，与模型朝向解耦，参数更直观 |
| **速度位置差分 (Base Spring)** | 替代累加，避免长时间的数值漂移 |
| **CUtlLinkedList 懒分配** | 只对实际有 jiggle 的骨骼分配状态，节省内存 |
| **后处理而非混合** | 保持与现有动画系统的解耦，不影响主循环复杂度 |

---

## 与引擎其他系统的关系

* **依赖**：tier0（数学库 Vector/matrix3x4_t）、tier1（CUtlLinkedList）、studio.h（模型数据）
* **不影响**：网络同步、服务器逻辑（仅在客户端执行、纯视觉效果）
* **不冲突**：与 IK、布娃娃、表情混合等系统共存（后处理覆盖，最后写矩阵者生效）
* **非确定性**：基于浮点积分和帧率相关，不同机器上可能有细微视觉差异

```
+---------------------------------------------------+
|               C_BaseAnimating                     |
|    StandardBlendingRules()                        |
|         |                                         |
|         +-- 骨骼 Blend / 四元数插值               |
|         +-- IK / 瞄准骨骼 / 扭曲骨骼              |
|         +-- JiggleBone  <--- 此行代码              |
|         +-- 布娃娃物理覆盖（如激活）               |
|         |                                         |
|         v                                         |
|    骨骼矩阵 -> 蒙皮 -> 渲染                      |
+---------------------------------------------------+
```

---

## 参考资料

* Michael Booth, Turtle Rock Studios - JiggleBone 设计与实现
* `src/public/jigglebones.h`
* `src/public/jigglebones.cpp`
* `src/game/client/c_baseanimating.cpp`
* `src/public/studio.h` (mstudiojigglebone_t)
