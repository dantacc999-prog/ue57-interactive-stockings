# UE 5.7 接入步骤

## 插件与工程

把 `Plugins/StockingsInteraction` 放入工程 `Plugins`，重新生成工程文件并编译。依赖模块为 Core、CoreUObject、Engine、InputCore、UMG、Slate、SlateCore、ProceduralMeshComponent、RenderCore、RHI；插件描述会启用 ProceduralMeshComponent。

材质生成需要启用 **Python Editor Script Plugin** 和 **Editor Scripting Utilities**。它们只用于编辑器生成资产，不属于插件运行时依赖。

```powershell
& "C:/UE_5.7/Engine/Build/BatchFiles/RunUAT.bat" BuildPlugin `
  -Plugin="C:/Projects/ue57-interactive-stockings/Plugins/StockingsInteraction/StockingsInteraction.uplugin" `
  -Package="C:/Build/InteractiveStockings" -TargetPlatforms=Win64
```

## 角色组件

1. 角色 Actor 上添加身体和连裤袜 SkeletalMeshComponent，二者保持相同位置、旋转、缩放。
2. 给身体播放原角色动画；袜子组件设置身体为 Leader Pose。身体和袜子的骨骼索引必须兼容。
3. 如果模型把鞋、裙子等分在不同材质槽，记录各槽位索引。若没有这些部件，将相关槽位改成自己的映射，并关闭相应按钮/开关。
4. 网格参考空间必须是 Z 向上，尺寸使用厘米。当前骨骼代码使用 `Hips`、`UpperLeg_L/R`、`LowerLeg_L/R`、`Foot_L/R`，新角色名称不同需修改 `StockingsStandingPose.cpp`、`StockingsVisualAtlas.cpp` 和 `CanInteract()`。
5. 启用 CPU Access，并在最终打包目标测试 LOD0 读取。当前代码按 LOD0 构造，动态换骨架或 LOD 要重新初始化。

## 使用最小宿主 Widget

无需原项目的聊天系统。创建继承 `StockingsViewerWidget` 的 Widget Blueprint，或直接在蓝图 `Create Widget` 中选择该 C++ 类。

Level Blueprint / PlayerController 的 BeginPlay：

```text
Create Widget (StockingsViewerWidget, Owning Player = 当前 PlayerController)
  → Add to Viewport
  → Initialize Viewer (Body = 身体组件, Stockings = 袜子组件, Camera = 观察 CameraActor)
```

CameraActor 的初始位置/方向需要对准角色。`CameraBaseDistance` 是初始镜头到旋转焦点的距离，默认 147 cm，焦点由初始相机位置加 Forward×该距离得到；按角色比例设置。

宿主提供白/黑/无袜切换、鞋开关、镜头复位。互动按钮与青色袜口点由 Interaction Actor 创建。滚轮在 Widget 中处理，中键和右键每帧交给共享相机库。

## Interaction Actor

在地图中放置 `StockingsInteractionActor`，设置 `ViewerWidgetClass` 为上述 Widget 类。不要再填写原项目中的私有 Widget 路径。

| Actor 属性 | 建议绑定 |
|---|---|
| OrdinaryWhiteMaterial | `/Game/InteractiveStockings/MI_White_Ordinary` |
| OrdinaryBlackMaterial | `/Game/InteractiveStockings/MI_Black_Ordinary` |
| WhiteDragMaterial | `/Game/InteractiveStockings/MI_White_Interactive` |
| BlackDragMaterial | `/Game/InteractiveStockings/MI_Black_Interactive` |
| WhiteSkinMaterial | 接好白色透肤函数的身体材质实例 |
| InteractionSkinMaterial | 接好黑色透肤函数的身体材质实例 |
| BodyMaterialSection | 身体皮肤槽，示例 2 |
| InnerGarmentMaterialSection | 泳装与侧带/扣环槽，示例 4 |
| Skirt/Socks/ShoesMaterialSection | 示例 6/7/8；与宿主字段保持一致 |
| InnerGarmentBoundsMin/Max | 参考空间范围，示例 (-11,-10,55) 到 (11,5,70) cm |

修改槽位后，还要设置宿主 Widget 的 `BodySection`、`TopSection`、`SkirtSection`、`SocksSection`、`ShoesSection`。皮肤恢复与鞋开关都使用这些索引。

参考范围应覆盖泳装本体、所有带子和扣环，同时排除尾巴、翅膀、上衣等同槽部件。模型比例或骨骼不同不要照搬上述数字。

## 使用现有 UMG

可以保留自己的 UI，只要提供同名字段与事件。最完整的字段定义在 `StockingsViewerWidget.h`，Actor 用反射按名称访问：

| 字段/事件 | 类型与作用 |
|---|---|
| SpeechMouthMesh / StockingsMesh | SkeletalMeshComponent；身体和袜子 |
| WardrobeOutlineMesh | 可选 SkeletalMeshComponent；外层描边同步隐藏 |
| StockingsOriginalBody | MaterialInterface；进入互动时的原身体材质 |
| StockingsState | Integer：0 无袜，1 白，2 黑 |
| ChatUIActive | Boolean；UI 可用 |
| DuckSitSelected | Boolean；为 true 时禁用互动 |
| WardrobeShoesWorn | Boolean；鞋内遮罩与显示状态 |
| CycleStockings / ApplyWardrobeVisibility | 无参数事件/函数；切换袜子和恢复衣物显示 |
| CameraViewActor / CameraReady | Actor / Boolean；观察镜头已初始化 |
| CameraModeLocation / CameraForward / CameraRight / CameraUp | Vector；初始镜头与方向基准 |
| CameraBaseDistance / CameraZoomFactor | Float；初始距离与缩放倍率 |
| CameraOrbitYaw/Pitch、CameraOrbitCurrentYaw/Pitch | Float；目标与平滑后的角度 |
| CameraPanOffset、CameraPanX/Y | Vector / Float；焦点偏移 |
| CameraMouseX/Y、CameraDragButton、CameraDragging | Float / Integer / Boolean；拖动状态 |

UI 根节点需为 CanvasPanel，插件才能添加互动按钮。`WardrobeModeButton` 可供复制按钮样式；`WardrobeModeLabel`、`CameraDragArea` 可选。共享镜头默认只从屏幕左侧 65.5% 区域开始拖动，以避开原聊天输入区；全屏观察器可在相机库中调整这一范围。

## 状态和输入

- 左键抓袜口；中键旋转；右键平移；滚轮缩放。旋转映射为 yaw+=DX×.25，pitch-=DY×.25，俯仰限制 ±80°。
- 最小距离 20 cm；缩放倍率 .08..2.2；世界平移最大 250 cm。相机插值沿角度和半径进行，避免插值弦线穿过角色。
- 模式开启时等两个更新帧，随后读取蒙皮数据。互动期间固定下肢贴合姿态；坐姿或明显折腿状态禁用互动。
- 换颜色或退出互动会恢复原网格、身体材质和原 Post Process Anim Instance。用 `SetRemovalProgress(0)` 复位当前互动服装。
- 单地图放置一个 Interaction Actor 和一个宿主 Widget。大量角色并发、动态骨骼替换、网络同步需要自行扩展。
