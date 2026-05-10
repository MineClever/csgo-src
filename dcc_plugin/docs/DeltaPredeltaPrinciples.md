# Delta / PreDelta 计算核心原理

本文说明 Maya DMX/SMD 插件里当前已实现的 delta、predelta 和 DMX `deltaStates` 的核心计算模型。对应实现主要在：

- `dcc_plugin/src/common/SourceDeltaUtils.h`
- `dcc_plugin/src/common/AnimationSampleUtils.cpp`
- `dcc_plugin/src/importer_dmx/DmxImportAnimation.cpp`
- `dcc_plugin/src/importer_smd/SmdSourceDeltaProcessor.cpp`
- `dcc_plugin/src/importer_dmx/DmxImportDeformers.cpp`
- `dcc_plugin/src/exporter_dmx/DmxExportDeformers.cpp`

## 术语

- authored sample：源 DMX/SMD 文件中读出的动画样本，记为 `V`。
- reference sample：从 Maya 当前场景或指定 animation layer 采样得到的参考值，记为 `R`。
- output sample：最终写入 Maya animCurve 或 animation layer 的值，记为 `O`。
- translation delta：平移向量差值。
- rotation delta：四元数差值，最后会按 Maya 节点当前 rotate order 展开成 Euler 并写入 `rotateX/Y/Z`。
- source delta：动画导入时的差值模式，覆盖 `subtract`、`presubtract`、`lineardelta`、`splinedelta`。
- DMX `deltaStates`：mesh / blendShape 几何目标的顶点偏移，不等同于动画 source delta。

## Source Delta 总流程

1. 先从 DMX `Dme*LogLayer` 或 SMD `skeleton` frames 中读取 authored samples。
2. 如果存在导入轴向或自定义 transform correction，先对顶层节点样本做导入侧校正。
3. 根据 `sourceDeltaMode` 决定是否把 authored samples 转为相对值。
4. 如果导入目标是 Maya animation layer 且模式是 `subtract` / `presubtract`，当前实现不会提前做差值，而是保持 authored samples 交给 additive layer 语义处理，避免二次减法。
5. 最终把样本写入 base animCurve 或 animation layer。

## subtract

`subtract` 表示“源样本减参考样本”。

平移：

```text
O = V - R
```

旋转：

```text
O = V * inverse(R)
```

含义：输出的是从参考姿态 `R` 到源姿态 `V` 的增量。

## presubtract

`presubtract` 表示“参考样本减源样本”，也就是反向差值。

平移：

```text
O = R - V
```

旋转：

```text
O = inverse(R) * V
```

注意：旋转不是简单交换减号，而是改变四元数乘法顺序。四元数乘法不满足交换律，所以 `V * inverse(R)` 和 `inverse(R) * V` 表达的是不同空间顺序下的相对旋转。

## reference 的来源

`subtract` / `presubtract` 有两种 reference 来源。

### 当前场景逐帧参考

当 `sourceDeltaUseClip=0` 或没有指定可用 scene clip 时：

```text
R[i] = sample_scene_at_time(target, time[i])
O[i] = delta(V[i], R[i])
```

这会在每个输入 key 的时间点直接采样目标节点当前场景值。

### 指定 animation layer 参考

当 `sourceDeltaUseClip=1` 且 `sourceDeltaClip` 指向可用 animation layer 时：

```text
Rclip = sample_layer_at_time(target, time[referenceFrame])
O[i] = delta(V[i], Rclip)
```

也就是说当前实现会从指定 layer 中取 `Reference Frame` 对应的一帧作为统一参考值，而不是用 layer 的每一帧逐帧相减。

## linearDelta

`linearDelta` 不读取 Maya 场景 reference。它用 authored samples 的首尾值构造一条线性参考曲线。

平移：

```text
t = i / (sampleCount - 1)
R[i] = lerp(V[0], V[last], t)
O[i] = V[i] - R[i]
```

旋转：

```text
t = i / (sampleCount - 1)
R[i] = slerp(V[0], V[last], t)
O[i] = V[i] * inverse(R[i])
```

用途：剥离一段动画从起点到终点的整体线性趋势，只保留中间偏移。

## splineDelta

`splineDelta` 与 `linearDelta` 使用同样的首尾参考模型，但会先把线性 `t` 变成平滑权重：

```text
t2 = 3t^2 - 2t^3
```

平移：

```text
R[i] = lerp(V[0], V[last], t2)
O[i] = V[i] - R[i]
```

旋转：

```text
R[i] = slerp(V[0], V[last], t2)
O[i] = V[i] * inverse(R[i])
```

用途：用平滑曲线剥离首尾趋势，减少线性参考在段首段尾的硬过渡。

## animation layer 下的特殊规则

DMX 和 SMD 都支持把 transform 动画写入 Maya animation layer。对于 source delta，当前实现有一条关键规则：

```text
if writing_to_animation_layer and mode in {subtract, presubtract}:
    O = V
```

也就是不提前计算 `V - R` 或 `R - V`。原因是 source-delta layer 应保持 additive/overlay 语义；如果在写层前先减一次 reference，再让 Maya layer 自己叠加，就会产生二次减法。

`linearDelta` / `splineDelta` 仍会在写入前计算内部首尾参考差值，因为它们不依赖 Maya 场景或 layer reference。

## SMD 与 DMX 的差异

- SMD 动画时间来自 frame index，并按 `animationFps` 转换到 Maya UI time。
- DMX 动画时间来自 `Dme*LogLayer.times`，当前按秒写入。
- SMD 只处理 skeleton transform 动画。
- DMX 处理 `position`、`orientation`、标量 channel 和最小 facial `flexWeight`。source delta 当前主要作用在 transform `position/orientation`。
- 两者共用 `SourceDeltaUtils.h` 中的差值公式，并共用 `AnimationSampleUtils` 的场景 / layer 采样能力。

## DMX deltaStates 几何差值

DMX mesh 的 `deltaStates` 是另一套语义，服务于 blendShape / vertex delta。

导入时：

```text
targetPoint[index] = basePoint[index] + deltaPosition
```

导出时：

```text
deltaPosition = targetPoint[index] - basePoint[index]
```

只有发生变化的顶点会写入 `positions` 和 `positionsIndices`。这些数据用于重建 Maya blendShape target，不参与 source delta 的动画 reference 采样。

## 当前边界

- `subtract` / `presubtract` 的 scene clip 路径当前使用单个 `Reference Frame` 值作为整段参考。
- `lineardelta` / `splinedelta` 只基于 authored samples 首尾生成参考，不读取 Maya 场景。
- source delta 主要覆盖 transform 动画；facial/标量通道仍按各自现有路径写入。
- DMX `deltaStates` 是几何差值，不要和动画 source delta 混用。
