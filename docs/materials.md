# 纱网材质与透肤

## 生成黑白袜材质

`Tools/build_materials.py` 从 `Materials/black_gauze.hlsl`、`white_gauze.hlsl` 和 `presets.json` 创建四组父材质/实例。使用 Two Sided、Translucent、Unlit；Custom 输出 RGB 经 EyeAdaptationInverse 接 Emissive，A 接 Opacity，法线偏移接 World Position Offset。

脚本独立创建资产，不依赖原角色、私有父材质或外部下载。`DEST` 默认 `/Game/InteractiveStockings`；避免意外覆盖，它会在资产已经存在时停止。重新生成时选择新的 DEST。

预设来自最终展示实例。独立生成器把角色专属密度/粗糙贴图替换为可调常量：白色 `OpticalDensity=.30`，黑色 `.22`，`WeaveRoughness=.5`，`SkinControls=0`。要贴近自己的模型效果，可把这些 Custom 输入替换为自己的 UV 贴图；这些贴图必须有使用权，仓库不再分发原角色贴图。

## Custom 关键输入

| 输入 | 来源 |
|---|---|
| MacroNormal / FiberTangentWS | VertexNormalWS / VertexTangentWS；程序网格重算并提供切线 |
| ViewDirection / WorldPosition | CameraVectorWS / WorldPosition |
| FabricUV | 原袜子 UV0 |
| FabricDensity（互动） | VertexColor.R；解码 R×2.4 |
| FabricDensity（普通） | WornFiberDensity=1/2.4，白色再估计姿态面积变化 |
| ActorOrigin | 组件局部零点转换到世界 + (0,0,60)，普通与互动使用同一锚点 |
| ToePosition | PreSkinnedPosition → VertexInterpolator.VS → PS |
| Density | 光学密度纹理或 OpticalDensity 常量 |
| SkinControls | R 控制局部透肤，B 控制加厚；默认零 |

PreSkinnedPosition 是顶点阶段数据，必须通过 VertexInterpolator 传到像素阶段。互动程序网格中此数据来自该网格当前局部位置；鞋尖区本来保持在脚部，因此遮罩适用。不能把它视为任意变形后都不变的原始骨骼位置。

## 织物、透明度和高光

UV 乘以 MeshColumns/Rows 得到经纬格点。每根纤维有平滑截面，交点的上下关系交替。纤维覆盖形成孔隙，法线偏移和双方向切线高光体现线束；远处用 `fwidth` 将格纹过滤成平均覆盖，降低摩尔纹和闪烁。

透光率采用 `opacity=1-exp(-depth*fiberAmount)`。互动密度来自实际表面积；白色另外把 Density 映射到 .24..38 的光学范围，以保持薄纱透肤。

| 最终主要参数 | 白色 | 黑色 |
|---|---|---|
| FabricTint | (.78,.79,.80) | (.012,.016,.020) |
| SheerDensityScale | 1.2 | 3.0 |
| MeshColumns / MeshRows | 3500 / 4500 | 见 presets.json |
| MeshRadius / MeshContrast | .14 / .78 | 见 presets.json |
| Roughness / FiberSpecular | .58 / .11 | 见 presets.json |

调粗纤维要同时检查近景孔隙与远景平均覆盖。增大整体 Opacity 会抹掉织物透肤；应先调厚度、纤维半径、光学密度和高光，再检查背面光域。

## 身体透肤函数

纱网孔隙透出的是真实身体网格。把 `skin_under_gauze.hlsl` 接到自己有授权的皮肤父材质里，建立黑/白两个实例。它是可移植函数，不包含原角色卡通皮肤图。

| 函数输入 | 接法 |
|---|---|
| Original | 你自己的原皮肤辐射/着色结果，曝光处理前的适当线性域 |
| SkinAlbedo / SkinColor | 身体底色贴图 RGB / 原皮肤颜色参数 RGB |
| NormalWS / PositionWS | VertexNormalWS / WorldPosition |
| LightDirection | 原项目或自己定义的世界补光方向 |
| ComponentOriginWS | Local (0,0,0) TransformPosition 到 World |
| CoveredUV | 自己身体 UV 的袜区遮罩：袜区1、手部等非覆盖区0 |
| RestPosition | PreSkinnedPosition 经 VertexInterpolator |
| SkinCoverageTopZWS | Scalar Parameter，默认很低；互动每帧设置袜口最高世界 Z |
| StaticCoverage | Scalar Parameter，默认0；普通状态 Actor 设置1 |
| WhiteVariant | 黑色实例0、白色实例1 |

Custom 输出 float3。原皮肤着色和本函数的输出必须处在同一色彩/曝光域；最终皮肤 Emissive 按你的管线处理一次曝光，避免重复处理。白色专用方向光使用组件原点+(-95,130,160)，和袜面组件锚点+(-95,130,100)+(0,0,60)一致。

普通静态覆盖在示例参考 Z72..73 处渐变，换比例需要改这个阈值。互动覆盖还乘 CoveredUV，防止只凭高度把手臂也染成袜下皮肤。没有身体 UV 遮罩时，不要直接对整个皮肤套用该函数。

## 鞋内遮罩

袜面 HLSL 用 `ShoesWorn` 遮住低趾尖：局部 Z3.1..3.5，局部 Y10.5..12。身体使用 `shoe_skin_mask.hlsl`，乘到自己的皮肤 Opacity，仅按低脚底高度过滤，因为身体和袜子的参考 Y 范围可能不同。

这些坐标来自展示角色。新鞋型、不同脚尖方向或模型比例必须重新测量；脱鞋模式应设 ShoesWorn=0 并确认趾尖完整恢复。
