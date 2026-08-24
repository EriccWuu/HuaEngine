# HuaEngine HLSL、SPIR-V、ShaderInterface 与材质系统演进 Spec

状态：已完成（SH0-SH6 及独立审查修复 R1-R5 已实现，最终验证记录见对应实施计划）

日期：2026-08-24

基线提交：`4b5f490 feat: update`

审查修复说明：针对首轮独立代码审查发现的 8 个 MAJOR 与 2 个 MINOR，已补齐严格 Shader Artifact/反射契约、后端中立 ShaderProgram、Pipeline layout 一致性校验、OpenGL fence 失败安全退路、内容寻址事务式 last-good、通用导入 DAG，以及 Inspector 的参数范围、覆盖项清理提示和导入健康状态。执行明细与验证命令见 `.workspace/superpower/plans/2026-08-24-hlsl-spirv-review-remediation-plan.md`。

## 1. 背景

HuaEngine 当前已经完成 Shader 资产注册、Shader GUID 引用、Shader Artifact、Material Artifact 和启动时内容哈希重导。材质源文件通过 `shader_guid` 引用 Shader 资产，运行时不再由 Material 直接读取 Shader 文件路径。

但现有 Shader 与 Material 主路径仍有以下结构性限制：

1. Shader 源资产仍是组合 GLSL 文件，Shader Artifact 仅保存顶点和片元 GLSL 文本。
2. Shader 在运行时由 OpenGL 编译，没有统一的离线编译、编译诊断和中间表示。
3. RHI 的 `BindGroupLayout` 不是由 Shader 接口生成。Material 当前根据自身参数排序，临时推导 binding 和 texture slot。
4. OpenGL 的 Frame、Material、Object 参数仍主要通过逐项 `glUniform*` 提交。RHI 虽已有 `UniformBuffer`、buffer range 和 bind group contract，但 OpenGL `SetBindGroup()` 尚未真正消费 `GpuBuffer` binding。
5. SPIR-V/DXIL 中的资源布局、常量缓冲布局、输入输出接口没有被提取为稳定的跨后端数据模型。
6. Inspector 只能选择 Material 资产，不能根据材质定义显示和编辑实体材质参数 override。
7. 当前资产新鲜度只包含根源文件内容哈希、importer 版本和 artifact 版本。Shader 接口变化时，源文件未变化的 Material 不会自动失效。

D3D12 接入将继续后置。短期内 OpenGL 仍是唯一运行后端，但 Shader 工具链应提前采用能够服务未来 D3D12 的 HLSL 主线。

## 2. 决策摘要

本阶段采用以下主线：

```text
HLSL source
    |
    | DXC -spirv
    v
SPIR-V stage modules
    |---------------------------> SPIR-V reflection -> ShaderInterface
    |
    | SPIRV-Cross
    v
Generated OpenGL GLSL
    |
    v
OpenGL compile/link/runtime
```

未来接入 D3D12 时使用：

```text
同一份 HLSL source -> DXC -> DXIL -> D3D12 PipelineState
```

关键决策如下：

1. HLSL 成为新的主要 Shader 创作语言。
2. DXC 生成的 SPIR-V 作为当前离线编译中间表示和 Shader 接口反射来源。
3. OpenGL 首版不直接加载 DXC 生成的 SPIR-V；使用 SPIRV-Cross 生成桌面 GLSL，再走现有 OpenGL 编译链接路径。
4. 不使用 glslang 的 HLSL frontend。HLSL 编译统一由 DXC 负责。
5. Shader Artifact 同时保存 SPIR-V、生成的 OpenGL GLSL、`ShaderInterface`、编译配置和稳定签名。
6. Frame、Material、Object 常量提交迁移到真实 UBO，不继续围绕生成 GLSL 维护 `glUniform*` 特例。
7. Material 不再决定 binding。Material 只提供 Shader GUID、逻辑参数值和实体 override；binding、offset、visibility 由 `ShaderInterface` 决定。
8. Inspector 依赖稳定的 `MaterialDefinition` 查询接口，不直接解析 YAML、HLSL、SPIR-V 或 Artifact 二进制。
9. Shader 的编辑器展示元数据由 `.shader` 描述资产显式提供；编译器反射只负责 GPU 接口，不推断 Color、Range、DisplayName 等编辑器语义。
10. HLSL 的 DXIL register 与 SPIR-V descriptor binding 分别显式声明；禁止依赖 `b/t/s` register 类型形成独立 SPIR-V binding 命名空间。
11. 首版 Sampler 不作为 Material 参数暴露；引擎为反射出的 Sampler binding 提供固定默认 Sampler，Artifact 保存 Texture/Sampler 到 OpenGL combined sampler 的映射。
12. Shader 的通用 Import Fingerprint 与传递 include 跟踪在 Shader 编译链落地时同步完成，不后置到 Material V2。
13. UBO Arena 复用必须依据真实 GPU 完成状态；OpenGL backend 需要使用 `GLsync`，不能把 command replay 完成当作 GPU fence 完成。

## 3. 目标与非目标

### 3.1 必须完成

- 支持 `.shader` 描述资产和 HLSL 源文件输入。
- 使用带 SPIR-V 后端的固定版本 DXC，在导入阶段编译 vertex/fragment stage。
- 使用 SPIRV-Cross 生成当前 OpenGL 后端可编译的 GLSL。
- 从 SPIR-V 生成稳定的 `ShaderInterface`。
- Shader Artifact V2 保存编译产物、接口和签名。
- OpenGL 使用真实 UBO 消费 Frame、Material、Object 常量 bind group。
- Pipeline 与 BindGroupLayout 由 `ShaderInterface` 驱动并进行一致性校验。
- Material Artifact V2 以 Shader Interface 为参数 schema，Material 不再手工声明 binding/texture slot。
- Shader 接口变化能够使依赖 Material 自动重新导入。
- Inspector 能显示 Material 参数，并编辑 `MaterialComponent::Overrides`。
- 现有 builtin Shader、Material 和 TestProj Shader、Material 完成迁移。

### 3.2 明确不做

- 不实现 D3D12 device、swapchain、command list 或 DXIL runtime pipeline。
- 不实现 Vulkan 后端，也不直接把 OpenGL 作为 Vulkan SPIR-V 环境使用。
- 不实现 Shader permutation、宏组合爆炸管理、异步编译或远程 Shader 编译服务。
- 不实现 Shader hot reload；继续使用当前启动导入和显式 Reimport。
- 不实现 bindless、descriptor indexing、GPU-driven material 或商业级 pipeline cache。
- 不实现 ray tracing、mesh shader、tessellation 和 geometry shader。
- 首版不支持任意数组、嵌套结构体、`mat3`、runtime-sized resource array 和复杂 HLSL resource 类型。
- 不把所有 Material 编辑能力一次性扩展为完整 Material Asset Editor；首版聚焦实体 Inspector override。

## 4. 工具链与依赖

### 4.1 DXC

必须使用带 SPIR-V CodeGen 的固定版本 DXC。不能默认使用系统 PATH 或 Windows SDK 自带的 `dxc.exe`，因为 Windows SDK 分发版本可能只支持 DXIL，SPIR-V CodeGen 通常来自 DXC 官方 release 或 Vulkan SDK 分发。

首版固定使用 DXC 官方 `v1.9.2607` release，`dxc --version` 二进制身份为 `1.9.0.5402`。升级编译器必须同步更新 `Dependencies/DXC/VERSION`、import fingerprint 和 golden tests。

仓库应固定以下工具：

```text
Dependencies/
└── DXC/
    ├── bin/win-x64/dxc.exe
    ├── bin/win-x64/dxcompiler.dll
    ├── bin/win-x64/dxil.dll
    ├── include/
    ├── LICENSE
    └── VERSION
```

构建后将 Shader 编译所需二进制复制到引擎产物目录。工程不能依赖开发机预装 Vulkan SDK。

DXC 首版参数基线：

```text
-spirv
-fspv-target-env=vulkan1.2
-fspv-reflect
-fvk-use-gl-layout
-Zpc
-WX
-Ges
```

Debug 构建保留 Shader debug 信息和未优化诊断；Release 构建启用优化并可移除非必要 debug 信息。实际参数必须进入 Artifact 编译配置和 import fingerprint。

首版 `DxcShaderCompiler` 通过子进程调用仓库固定路径下的 `dxc.exe`，不从 `PATH` 查找。封装必须处理：

- 进程超时与异常退出。
- stdout、stderr 和退出码收集。
- UTF-8 diagnostic 解码。
- 每个 stage 独立的临时输入/输出文件生命周期。
- 编译器实际版本查询，并将结果写入 Artifact 和 Import Fingerprint。

后续只有在进程启动成本成为可测量瓶颈时，才考虑切换到 `dxcompiler.dll` API；该切换不得改变 `ShaderCompiler` 公共接口和 Artifact contract。

工具运行目录和进程约束固定为：

- 构建产物将 `dxc.exe`、`dxcompiler.dll`、`dxil.dll` 和 `VERSION` 复制到 `<ExecutableDirectory>/ShaderTools/DXC/`。
- `DxcShaderCompiler` 通过 executable directory 解析上述路径；文件缺失或 `VERSION` 与实际 `dxc --version` 不一致时拒绝编译。
- 每个 stage 的默认超时为 30 秒，stdout 与 stderr 各限制为 1 MiB；超时、输出超限和非零退出码均为编译失败。
- 临时目录为系统 temp 下的 `HuaEngine/ShaderCompile/<process-id>/<job-id>/`；job 完成后清理，启动时允许清理本进程不可能持有的遗留目录。
- 子进程 working directory 为 job 临时目录；所有 include root 使用规范化绝对路径显式传入，不依赖当前工作目录。

### 4.2 SPIRV-Cross

首版固定使用 `vulkan-sdk-1.4.357.0` tag（commit `6c09849fe88c48eaed08413aa022aaa136a3a057`）。

SPIRV-Cross 以固定提交的源码依赖静态构建，只启用 core、GLSL 和 reflection 所需模块，不构建 CLI 和不需要的 MSL/HLSL backend。

```text
Dependencies/
└── SPIRV-Cross/
```

使用 C++ API完成：

- SPIR-V 资源反射。
- descriptor set/binding、stage input/output、uniform buffer member offset/size 提取。
- separate texture/sampler 到 OpenGL combined sampler 的映射。
- 生成桌面 OpenGL GLSL。

### 4.3 SPIR-V 验证

每个 DXC 输出 stage module 必须通过 DXC 默认启用的 SPIR-V 验证。首版不额外引入独立 SPIRV-Tools 优化管线，也不允许使用 `-Vd` 关闭验证；后续只有在需要独立 `spirv-val` diagnostic 时再单独立项。

### 4.4 生成 GLSL 的离线验证

为保证 Shader 导入器在覆盖 last-good Artifact 前验证 OpenGL 目标，又不让 Asset importer 依赖 OpenGL context，固定版本 glslang 仅作为 **GLSL frontend validator** 静态接入：

首版固定使用 glslang `16.5.0`。

- 输入只允许 SPIRV-Cross 生成的 GLSL。
- 使用 `EShSourceGlsl` 和 OpenGL client semantics。
- 不使用已经 deprecated 的 glslang HLSL frontend。
- 验证失败视为 Shader import 失败，不提交新 Artifact。
- 运行时 OpenGL 仍执行驱动编译和链接，并保留完整 driver log。

```text
Dependencies/
└── glslang/
```

因此离线链路是：DXC 验证 HLSL/SPIR-V，SPIRV-Cross 生成 GLSL，glslang 验证生成 GLSL；三者均不创建 GPU resource，也不要求窗口或 OpenGL context。

## 5. Shader 资产模型

### 5.1 `.shader` 是用户可见资产

新的用户可见 Shader 资产使用 `.shader` 描述文件。HLSL 文件和 include 文件是该资产的导入输入，不单独出现在 Material Shader 选择器中。

示例：

```yaml
name: Sandbox
language: HLSL
source: shaders/sandbox.hlsl
stages:
  vertex:
    entry: VSMain
    profile: vs_6_0
  fragment:
    entry: PSMain
    profile: ps_6_0
parameters:
  u_Color:
    display_name: Color
    scope: Material
    editor: Color
    default: [1.0, 1.0, 1.0, 1.0]
  u_Texture:
    display_name: Texture
    scope: Material
    editor: Texture2D
```

约束：

- Material 只引用 `.shader` 资产 GUID。
- `.shader` 内部可引用相对 HLSL 路径；该路径只存在于导入阶段，不进入运行时 Material。
- include 必须限制在项目 `Assets/`、引擎 builtin source root 或明确配置的 Shader include root 中，禁止目录逃逸。
- 导入器必须记录根描述文件、HLSL 和所有传递 include 的内容哈希。
- source、stage entry、profile 和编译参数变化都必须使 Shader Artifact 失效。

### 5.2 Binding 约定

首版固定空间约定：

| HLSL register space | BindGroup slot | Scope | 用途 |
|---|---:|---|---|
| `space0` | 0 | Frame | 相机、帧级公共数据 |
| `space1` | 1 | Material | 材质常量、纹理、采样器 |
| `space2` | 2 | Object | 变换与对象级数据 |

示例：

```hlsl
[[vk::binding(0, 0)]]
cbuffer FrameData : register(b0, space0)
{
    float4x4 u_ViewProjection;
};

[[vk::binding(0, 1)]]
cbuffer MaterialData : register(b0, space1)
{
    float4 u_Color;
};

[[vk::binding(1, 1)]]
Texture2D u_Texture : register(t0, space1);

[[vk::binding(2, 1)]]
SamplerState u_TextureSampler : register(s0, space1);

[[vk::binding(0, 2)]]
cbuffer ObjectData : register(b0, space2)
{
    float4x4 u_Transform;
};
```

DXC 的 SPIR-V backend 默认把 `register(xN, spaceM)` 映射为 `binding=N, set=M`，其中 register 类型 `b/t/s/u` 不形成独立 binding 命名空间。因此同一 space 内的 `b0`、`t0` 和 `s0` 会冲突。首版必须同时满足：

- `register(..., space...)` 表达未来 DXIL register contract。
- `[[vk::binding(binding, set)]]` 显式表达 SPIR-V descriptor contract。
- 同一 set 内所有常量缓冲、Texture 和 Sampler 的 SPIR-V binding 全局唯一。
- Descriptor 解析和反射合并都拒绝重复或冲突 binding，不做静默重映射。

首版要求每个 scope 最多一个常量缓冲。纹理和采样器使用独立逻辑 binding；OpenGL 后端可以组合，但不能改变公共 `ShaderInterface` 语义。

### 5.3 首版参数类型

首版保证支持：

- `int`
- `float`
- `float2`
- `float3`
- `float4`
- `float4x4`
- `Texture2D`
- `SamplerState`

首版 Material 参数限制为 `float`、`float2`、`float3`、`float4`、`Texture2D`。Frame/Object 使用 `float4x4`。其余类型遇到时导入失败并给出明确 diagnostic，不静默降级。

### 5.4 常量缓冲布局约定

- HLSL 编译固定使用 `-Zpc`，矩阵 authoring contract 为 column-major。
- CPU 侧不得把 C++/GLM 结构体整体 `memcpy` 到 UBO；必须逐成员按照 ShaderInterface 反射的 offset、size、array stride 和 matrix stride 写入。
- `float3` 等存在 padding 歧义的类型必须通过固定 golden shader 验证，不能依赖 `sizeof(glm::vec3)` 推导 GPU 布局。
- SH0 固定 SPIR-V 与 DXIL 反射 golden contract；SH2 引入固定 DXC 后生成真实反射结果并与 contract 比较。若同一声明在两端布局不同，必须通过 authoring 限制或显式 padding 消除差异，不能在运行时后端各自解释 Material 数据。
- `-fvk-use-dx-layout` 不作为默认参数，除非 golden shader 证明其结果可由目标 GLSL 330 和 glslang OpenGL semantics 稳定表达；启用时必须进入编译配置和 Import Fingerprint。
- 首版固定使用 `-fvk-use-gl-layout` 生成可由 GLSL 330 `std140` 表达的布局；导入器同时模拟 HLSL register packing，二者 offset 不一致时拒绝导入并要求显式 padding。

首版 golden constant buffer 固定为：

```hlsl
[[vk::binding(0, 0)]]
cbuffer FrameData : register(b0, space0)
{
    float4x4 u_ViewProjection;
};

[[vk::binding(0, 1)]]
cbuffer MaterialData : register(b0, space1)
{
    float u_Roughness;
    float _Padding0;
    float2 u_UvScale;
    float3 u_Emissive;
    float u_Alpha;
    float4 u_Color;
};

[[vk::binding(0, 2)]]
cbuffer ObjectData : register(b0, space2)
{
    float4x4 u_Transform;
};
```

| Buffer/member | Offset | Size | Matrix stride | 说明 |
|---|---:|---:|---:|---|
| `FrameData.u_ViewProjection` | 0 | 64 | 16 | column-major `float4x4` |
| `MaterialData.u_Roughness` | 0 | 4 | 0 | scalar |
| `MaterialData._Padding0` | 4 | 4 | 0 | 显式消除 HLSL register packing 与 std140 差异 |
| `MaterialData.u_UvScale` | 8 | 8 | 0 | `float2` |
| `MaterialData.u_Emissive` | 16 | 12 | 0 | `float3` 从新 16-byte register 开始 |
| `MaterialData.u_Alpha` | 28 | 4 | 0 | 复用 `float3` 尾部位置 |
| `MaterialData.u_Color` | 32 | 16 | 0 | `float4` |
| `ObjectData.u_Transform` | 0 | 64 | 16 | column-major `float4x4` |

`FrameData`、`MaterialData`、`ObjectData` 的 buffer size 分别为 64、48、64 bytes。ShaderInterface 的 member `Size` 表示逻辑值占用字节数，buffer padding 通过下一个 member offset 和 buffer size 表达。

## 6. ShaderInterface

### 6.1 数据模型

新增后端无关数据结构：

```cpp
struct ShaderGpuInterface {
    std::vector<ShaderStageInterface> Stages;
    std::vector<ShaderVertexInput> VertexInputs;
    std::vector<ShaderResourceBinding> Resources;
    std::vector<ShaderConstantBuffer> ConstantBuffers;
    std::array<uint8_t, 32> Digest{};
    uint64_t Signature = 0;
};

struct ShaderAuthoringMetadata {
    std::vector<ShaderParameterMetadata> ExposedParameters;
    std::array<uint8_t, 32> MaterialDefinitionDigest{};
    uint64_t MaterialDefinitionSignature = 0;
};

struct ShaderInterface {
    ShaderGpuInterface Gpu;
    ShaderAuthoringMetadata Authoring;
};
```

`ShaderGpuInterface` 是 RHI、Pipeline 和 BindGroupLayout 可见的唯一部分。`ShaderAuthoringMetadata` 只服务 Artifact、MaterialDefinition 和 Editor，不进入 `ShaderProgramDesc`，避免 UI 语义泄漏到渲染后端。

核心子结构至少表达：

- stage 与 entry point。
- vertex semantic、location、格式。
- bind group slot、binding、resource type、array count、stage visibility。
- constant buffer size。
- constant member name、逻辑类型、offset、size、matrix layout。
- Material parameter editor metadata 和默认值。
- stable interface signature。

### 6.2 接口合并规则

vertex 与 fragment stage 分别反射后再合并：

- 相同 set/binding 必须具有相同资源类型。
- 相同 constant buffer 的 size 和 member layout 必须一致。
- stage visibility 取并集。
- vertex output 与 fragment input 的 location/type 必须匹配。
- `.shader` 中声明为 Material exposed 的参数必须在 `space1` 反射结果中存在。
- Frame/Object 参数不能出现在 Material Inspector。
- 未在 `.shader` 元数据中声明的 Material GPU 参数仍可参与绑定，但默认不暴露给 Inspector。

### 6.3 签名

`ShaderGpuInterface::Signature` 必须由规范化 GPU 接口计算，至少包含：

- stage/entry。
- vertex input location 和格式。
- set、binding、resource type、array count 和 visibility。
- constant buffer size、member name/type/offset/size。
- matrix row/column major、array stride 和 matrix stride。

DisplayName、tooltip、默认展开状态等纯 UI 信息不进入 GPU layout signature；参数默认值和 editor 类型进入 Material definition signature。

签名输入必须使用带字段长度和固定字节序的 canonical binary encoding，禁止直接拼接无边界字符串。所有集合在编码前按稳定 key 排序；签名算法首版固定，不得依赖 `std::hash`、容器遍历顺序或平台 ABI。Artifact 同时保存 canonical schema version，改变编码规则时必须提升该版本。

首版 canonical signature contract 固定为：

- schema version 为 `1`。
- 复用 `AssetBinaryWriter` 的 little-endian `u8/u32/u64` 和 `u32 byteLength + UTF-8 bytes` 字符串编码。
- stage 按 stage enum、entry 排序；vertex input 按 location、semantic 排序；resource 按 set、binding、resource type 排序；constant buffer 按 set、binding 排序；member 按 offset、name 排序。
- enum 写入显式稳定 wire value，不直接序列化 C++ enum 内存。
- canonical bytes 使用 SHA-256。Artifact 保存完整 32-byte digest；运行时 `uint64_t Signature` 取 digest 前 8 bytes，并按 big-endian 解释为整数，仅用于快速比较和 cache key。
- Artifact compatibility、Material stale 判断和导入依赖使用完整 digest；只有完整 digest 相同才视为接口兼容，不能仅比较 64-bit Signature。

GPU digest 不包含参数默认值、editor kind、range、step、DisplayName 和 tooltip。Material definition digest 以完整 GPU digest 为前缀，追加 exposed parameter 的 name、logical type、default value、editor kind、range 和 step；DisplayName 与 tooltip 只影响展示，不触发 Material 重导。

## 7. Shader Artifact V2

Shader Artifact V2 保存：

```text
ShaderArtifactDataV2
├── SourceLanguage = HLSL
├── CompilerIdentity
│   ├── DXC version
│   ├── SPIRV-Cross version
│   └── compile options
├── StageArtifacts[]
│   ├── stage
│   ├── entry point
│   ├── profile
│   ├── SPIR-V words
│   └── generated OpenGL GLSL
├── ShaderInterface
├── InterfaceDigest
├── InterfaceSignature
├── OpenGLBindingMap
│   ├── uniform block binding points
│   └── texture/sampler -> combined sampler entries
└── ImportInputs[]
    ├── normalized relative path
    └── SHA-256
```

约束：

- 运行时 OpenGL 不读取 `.shader` 或 `.hlsl`。
- Artifact 解码必须限制 stage 数、字符串长度、SPIR-V 大小、资源数和参数数。
- Artifact 中的 generated GLSL 必须已通过导入阶段的 glslang OpenGL semantics 验证。
- Debug diagnostic 可以保存 source path/line 映射，但 runtime 不依赖原始文件存在。
- Shader Artifact V1 保留只读迁移能力；现有 GLSL 资产迁移完成后可在独立阶段删除。
- Shader V2 Artifact 使用内容寻址文件名，先写入并回验新候选文件，再原子切换 Library record；任一步失败都不得覆盖或删除 last-good Artifact。
- `ImportInputs` 与实际 Import Fingerprint 使用同一份规范化输入集合，禁止分别维护两套 include 列表。

## 8. OpenGL 生成与运行

### 8.1 不直接加载 DXC SPIR-V

DXC SPIR-V 以 Vulkan target environment 生成。OpenGL 有独立 SPIR-V execution environment，两者在 binding 和资源数组等规则上存在差异。因此首版不调用 `glShaderBinary(... SPIR_V ...)` 直接加载 DXC 产物。

SPIRV-Cross 首版固定输出桌面 GLSL 330，与当前仓库 Shader 基线一致。UBO 通过运行时 `glUniformBlockBinding()` 关联 binding point，不依赖 GLSL 420 的 `layout(binding=...)`。超出 GLSL 330 能力的 HLSL/SPIR-V 功能在导入阶段失败，不能在运行时根据驱动临时改写 Shader 接口。

SH3 为保持旧 Material uniform 上传路径，生成 GLSL 临时启用 `emit_uniform_buffer_as_plain_uniforms`，并按 `FrameData`、`MaterialData`、`ObjectData` 结构成员解析 uniform location；ShaderInterface 和 Artifact 仍保存真实 cbuffer contract。SH4 接入 UBO 后必须关闭该兼容选项和 location fallback。

### 8.2 UBO 主路径

现有 RHI 已具备：

- `GpuBufferUsage::Uniform`
- `BindingValueType::UniformBuffer`
- bind group entry 的 offset/size
- `RenderDevice::UploadBuffer()`
- pipeline bind group slot 校验

需要补齐：

- OpenGL `SetBindGroup()` 对 `Ref<GpuBuffer>` 的 `glBindBufferRange(GL_UNIFORM_BUFFER, ...)` 消费。
- Shader program 创建时按 `ShaderInterface` 调用 uniform block binding。
- set/slot 与 binding 通过 Artifact 中的 `OpenGLBindingMap` 映射为稳定的 OpenGL uniform buffer binding point。
- Frame、Material、Object 数据按反射 offset 打包到 UBO。

OpenGL binding point 不直接使用 `set * 常量 + binding` 之类未经设备上限校验的公式。导入阶段按 `(set, binding)` 稳定排序生成稠密逻辑索引；创建 ShaderProgram 时查询并校验设备的 uniform buffer binding 上限，再把逻辑索引绑定到实际 binding point。超出上限必须拒绝 program/pipeline 创建并给出所需数量和设备上限。

OpenGL 后端私有资源映射的编号规则固定为：

- Uniform block entries 按 `(set, binding)` 排序，依次分配逻辑 uniform binding point `0..N-1`。
- Combined sampler entries 按 `(textureSet, textureBinding, samplerSet, samplerBinding)` 排序，依次分配逻辑 texture unit `0..M-1`。
- 相同 texture/sampler pair 跨 stage 合并，visibility 取并集；不同 pair 即使引用同一 Texture 也占独立 combined sampler entry。
- generated GLSL uniform/block name 只作为 OpenGL program link 后的定位信息，不参与公共 binding 身份；公共身份始终是 set/binding pair。
- 公共 RHI 只保存 set/binding 逻辑身份，不保存稠密编号、`GLuint`、uniform location 或实际上限。OpenGL ShaderProgram 创建时按逻辑接口生成私有稠密映射，并完成设备校验和 location 解析。

首版建立 frame-local `UniformBufferArena`：

- 每个 frame-in-flight 使用独立 backing buffer 或安全区间。
- 按 `GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT` 对齐子分配。
- Frame 数据每相机分配一次。
- Object 数据每 draw 分配一次。
- Material 数据按本帧实际参数快照分配；不做跨帧复杂 cache。
- frame fence 完成前不复用其区间。

首版必须设置容量上限；不足时可以扩容并记录统计，但不能每个 draw 创建独立 OpenGL buffer。

当前 OpenGL queue 的 timeline fence 只表示 CPU command replay 已完成，不代表 GPU 已消费 buffer。SH4 必须用 `glFenceSync`/`glClientWaitSync` 实现真实 GPU completion，再允许 `UniformBufferArena` 根据 fence value 回收区间。同步对象必须在所属 OpenGL context 中创建、等待和销毁；context 关闭前清理所有未完成 sync。

OpenGL timeline fence contract 固定为：

1. 每次 graphics queue submit replay 完成后调用 `glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0)`，并把 sync 与递增 signal value 一起入队。
2. `GetCompletedValue()` 只从队首开始以 `glClientWaitSync(sync, 0, 0)` 非阻塞轮询；已 signaled 的 sync 依次删除并推进 completed value，未完成时立即停止。
3. `Submit(QueueSubmitDesc)` 的 wait fence 若未完成不得伪装成功；同一 OpenGL queue 的等待可以主动轮询或返回未提交，跨 backend/context fence 首版拒绝。
4. `UniformBufferArena` 的 frame segment 记录最后引用它的 signal value；只有 `GetCompletedValue() >= value` 时才能 reset 或复用。
5. context shutdown 在 context 仍有效时删除所有剩余 `GLsync`；context 丢失后只清理 CPU bookkeeping，不再调用 GL API。

首版不在普通 frame path 使用无限等待。Arena 容量不足时优先扩容到配置上限；达到上限后返回可诊断失败，由 renderer 跳过受影响 draw，不能覆盖未完成区间。

### 8.3 纹理和采样器

公共 ShaderInterface 保留独立 Texture 和 Sampler binding。SPIRV-Cross 生成 GLSL 时建立 combined image sampler，并输出从公共 pair 到 OpenGL sampler uniform 的映射。

首版 Sampler contract 固定为：

- `.shader` 可以声明 `SamplerState` GPU 资源，但不能把它标记为 Material exposed parameter。
- Material Source/Artifact 不保存 Sampler 值。
- 每个反射出的 Sampler binding 由运行时提供 canonical default sampler。
- Shader Artifact 保存 SPIRV-Cross `build_combined_image_samplers()` 产生的每个 texture/sampler pair；同一 Texture 被不同 Sampler 使用时保留多条 pair。
- 无法静态确定组合关系、缺少 Texture/Sampler 任一侧或使用首版不支持的 sampler 语义时，Shader 导入失败。

Canonical default sampler 等于当前 `SamplerDesc` 默认值：MinFilter=`Linear`、MagFilter=`Linear`、AddressU/V/W=`Repeat`。它由 RenderDevice 按设备生命周期创建一次并复用，不属于 Material 数据，也不进入 Material definition digest。未来增加可配置 Sampler 时必须提升 Material Artifact contract，不能改变该默认对象的既有语义。

OpenGL backend 负责：

- 将 TextureView 绑定到 texture unit。
- 将 Sampler 绑定到同一 texture unit。
- 设置 generated combined sampler uniform。
- 验证 ShaderInterface 所需 pair 均已提供。

Material 不保存 OpenGL texture unit。

## 9. RHI 与 Pipeline 收敛

### 9.1 ShaderProgramDesc

`ShaderProgramDesc` 从仅有 `VertexSource/FragmentSource` 扩展为显式 stage artifact：

```cpp
struct ShaderStageBinary {
    ShaderStage Stage;
    ShaderStageCodeFormat Format;
    std::string EntryPoint;
    std::vector<uint8_t> Code;
};

struct ShaderProgramDesc {
    std::vector<ShaderStageBinary> Stages;
    ShaderGpuInterface Interface;
};
```

OpenGL stage format 使用 generated GLSL text；未来 D3D12 使用 DXIL。`ShaderProgramDesc` 只携带目标代码和后端无关的逻辑 ShaderInterface；OpenGL backend 根据 set/binding 和生成代码建立私有的 texture unit、uniform location 与 uniform buffer binding point 映射。Artifact 中的 `OpenGLBindingMap` 属于 OpenGL 目标产物元数据，不再投影进公共 RHI。公开 RHI 不出现 DXC、SPIRV-Cross、GLuint 或 D3D12 原生类型，也不包含 DisplayName、tooltip、editor kind 等 authoring metadata。

### 9.2 BindGroupLayout 来源

- `CreateFrameBindGroupLayout()`、`CreateMaterialBindGroupLayout()`、`CreateObjectBindGroupLayout()` 改为从 ShaderInterface scope 提取。
- 删除 Material 参数排序决定 binding 的主路径。
- `MaterialBindingSchema` 若保留，只能是 ShaderInterface Material scope 的只读投影，不能再独立生成 GPU layout。
- PipelineState 创建时校验其 BindGroupLayout signature 与 ShaderInterface 一致。
- `SetBindGroup()` 继续校验 slot 和 layout；缺失资源在 draw 前失败并输出稳定 diagnostic。

## 10. Material 系统 V2

### 10.1 Material Source

Material Source V2 示例：

```yaml
name: SandboxMaterial
shader_guid: 71940333dd9141dc96651fe1efdc7d65
parameters:
  u_Color: [1.0, 1.0, 1.0, 1.0]
  u_Texture: Textures/Checker.png
```

变化：

- 不再要求 Material 重复声明 `value_type`。
- 删除 `texture_slots`。
- 参数类型、binding、offset 和默认值来自 ShaderInterface 与 Shader authoring metadata。
- Texture 源引用在导入时解析为 GUID，运行时 Material 不保存项目路径。
- Material 可以省略参数，省略时使用 Shader parameter default。
- 未知参数、类型不匹配、缺失必需资源必须使导入失败。

### 10.2 Material Artifact

Material Artifact V2 保存：

- Material name/type。
- Shader GUID。
- Shader interface full digest 和 64-bit runtime signature。
- 规范化后的参数值。
- Texture GUID。
- Material definition signature。
- Shader dependency GUID 和 Shader interface full digest。
- Texture dependency GUID；Texture 内容签名不进入 Material Artifact contract。

运行时解析时再次验证 Shader interface full digest。Digest 不匹配时不得建立错误的 bind group；应报告 artifact stale，并要求重新导入。

### 10.3 MaterialDefinition

新增只读查询模型：

```cpp
struct MaterialDefinition {
    AssetGuid MaterialGuid;
    AssetGuid ShaderGuid;
    std::array<uint8_t, 32> ShaderInterfaceDigest{};
    uint64_t ShaderInterfaceSignature;
    std::vector<MaterialParameterDefinition> Parameters;
};
```

每个参数至少包含：

- name/display name。
- logical type。
- default value。
- Material 当前值。
- editor kind、range、step、tooltip。
- Texture asset kind 限制。

Renderer、Inspector 和序列化层都可以消费该模型，但它不持有 GPU resource，不创建 ShaderProgram。

## 11. 导入依赖与 Import Fingerprint

当前 `SourceContentHash` 只覆盖根文件，不能表达 Shader descriptor、HLSL、include 和 Material -> Shader 接口依赖。

引入 `AssetImportFingerprint`：

```text
ImportFingerprint = SHA-256(
    importer id/version
    artifact version
    normalized import options
    root source hash
    ordered source input path/hash list
    ordered dependency guid/artifact-signature list
)
```

`AssetImportFingerprint` 的通用模型、canonical encoding 和 Library 持久化属于 SH1；Shader 的 source input 收集属于 SH2。SH5 只增加跨资产 dependency signature 和导入 DAG，不得等到 SH5 才让 HLSL include 参与 Shader 新鲜度判断。

### 11.1 Shader

Shader fingerprint 包含：

- `.shader` 内容。
- HLSL 内容。
- 所有传递 include 的相对路径和内容。
- DXC/SPIRV-Cross version。
- stage/profile/entry point。
- 编译 flags。
- target OpenGL GLSL version。

### 11.2 Material

Material fingerprint 包含：

- `.material` 内容。
- Shader GUID。
- Shader Artifact 的 interface full digest。
- 所有 Texture dependency GUID。

Texture 内容变化不会改变 Material 的参数布局或 Texture GUID，因此不触发 Material 重导；Texture 自身 Artifact 更新和 runtime cache invalidation 由 Texture 资产链路负责。

### 11.3 导入顺序

AssetImportService 必须按导入依赖拓扑执行：

1. 解析轻量级 source dependency metadata。
2. 构建 Shader -> Material 等导入 DAG。
3. 先导入或确认依赖 artifact current。
4. 再计算 dependent fingerprint 并导入 Material。
5. 循环依赖必须给出完整 GUID/path 链路 diagnostic。

不能依赖 manifest 的偶然遍历顺序。

导入计划必须先完整构建并验证 DAG，再开始提交新 Artifact。单个资产仍使用 last-good 原子提交；若后续 dependent 导入失败，已经成功提交的新 Shader Artifact可以保留，但对应 Material 必须保持 stale 状态且运行时不得建立签名不匹配的 BindGroup。

## 12. Inspector 材质参数

### 12.1 数据来源

Inspector 不直接访问 RHI ShaderProgram，也不解析 `.material`。Editor 通过 AssetService 的只读查询入口获取 `MaterialDefinition`。

```cpp
ResultEnvelope GetMaterialDefinition(
    const AssetGuid& materialGuid,
    MaterialDefinition& outDefinition);
```

该查询可以从 Material Artifact 和 Shader Artifact 解码数据，不要求 RHI 已初始化。

### 12.2 实体 Inspector 行为

选中带 `MaterialComponent` 的实体后：

- 先显示 Material 资产选择器。
- Material 有效时显示其 exposed parameters。
- 每项显示 override 开关或 reset 控件。
- 未 override 时显示 Material 当前值。
- 修改后写入 `MaterialComponent::Overrides`。
- reset 删除对应 override，而不是复制默认值。
- Material 切换后保留名称和类型均兼容的 override；不兼容 override 删除并给出一次性提示。

编辑器映射：

| 参数 | Inspector 控件 |
|---|---|
| Float | DragFloat，使用 range/step |
| Float2/3/4 | DragFloatN |
| Color | ColorEdit3/4 |
| Texture2D | 复用 AssetPicker，过滤 Texture2D |

所有编辑必须进入 Editor command/undo 链路并标记 Scene dirty。不能直接在 ImGui 绘制函数中绕过命令系统永久修改组件。

### 12.3 不支持状态

- Material GUID 缺失：显示 Missing Material，不清空原 GUID。
- Material Artifact stale：显示 Reimport required。
- Shader 编译失败：保留最后一个成功 Artifact；Inspector 显示 last-good 数据和编译失败状态。
- 参数类型暂不支持：只读显示类型，不允许产生无效 override。

## 13. 目录与模块边界

本阶段采用以下目录和模块边界：

```text
HuaEngine/src/HuaEngine/
├── Asset/
│   ├── Artifact/
│   │   ├── ShaderArtifact.*
│   │   └── MaterialArtifact.*
│   └── Import/
│       ├── ShaderAssetImporter.*
│       ├── ShaderDescriptor.*
│       └── AssetImportFingerprint.*
├── Rendering/
│   ├── Shader/
│   │   ├── ShaderCompiler.h
│   │   ├── DxcShaderCompiler.*
│   │   ├── ShaderInterface.*
│   │   ├── ShaderInterfaceReflection.*
│   │   └── SpirvCrossCompiler.*
│   ├── Material/
│   │   ├── MaterialDefinition.*
│   │   └── MaterialParameterPacker.*
│   └── RHI/
│       └── ShaderProgram.h
└── Platform/OpenGL/RHI/
    └── OpenGLRenderDevice.*

Editor/src/Panels/
├── MaterialParameterEditor.*
└── RuntimeInspector.*
```

边界规则：

- DXC、SPIRV-Cross 只存在于导入/编译模块，不进入 OpenGL backend。
- glslang 只验证生成 GLSL，不负责 HLSL 编译，也不进入 OpenGL backend。
- OpenGL backend 只消费 generated GLSL 和 ShaderInterface。
- Asset importer 不创建 GPU resource。
- MaterialDefinition 不依赖 RHI 初始化。
- Inspector 不依赖 Shader 编译器和 Artifact 二进制格式细节。

## 14. 阶段拆分

为避免与已有 D3D12 P71-P76 编号冲突，本 Spec 使用 `SH0-SH6`。每个 SH 阶段完成后独立提交。

### SH0：接口契约冻结

状态：已完成

内容：

- 固定 HLSL register 与 SPIR-V set/binding 双重声明规则。
- 固定 cbuffer matrix major order、offset、size、array stride 和 matrix stride contract。
- 固定首版 Texture/Sampler 配对和 canonical default sampler。
- 固定 OpenGL uniform block、texture unit 和 combined sampler 映射模型。
- 固定 canonical signature 编码、schema version 和 hash 算法。
- 固定 DXC 定位、子进程执行、diagnostic 和 last-good 原子提交 contract。

验收：

- 最小 Frame/Material/Object HLSL 样例不存在 set/binding 冲突。
- CPU packing contract 对每个首版参数类型给出明确 offset/size 预期。
- ShaderInterface、MaterialDefinition 和 RHI 的数据所有权边界无循环依赖。
- SH1-SH6 不再需要临时发明 binding、sampler、fingerprint 或 fence 规则。

冻结结果：

| 契约 | 固定结果 | 定义位置 |
|---|---|---|
| DXIL/SPIR-V binding | `register` + `vk::binding` 双重显式声明 | 5.2 |
| Constant buffer packing | `-Zpc`、反射 offset/stride、固定 golden layout | 5.4 |
| Interface identity | canonical little-endian bytes + SHA-256 full digest | 6.3 |
| OpenGL resource numbering | 稠密 logical UBO point 与 texture unit | 8.2 |
| Sampler | Linear + Repeat canonical default sampler | 8.3 |
| GPU completion | 每次 submit 建立 `GLsync`，按 timeline value 回收 | 8.2 |
| DXC execution | 固定 sibling tool path、30 秒超时、隔离 temp job | 4.1 |
| Last-good | 内容寻址候选 Artifact + Library catalog 原子切换 | 17 |
| Import freshness | 通用 fingerprint 在 SH1，Shader inputs 在 SH2 | 11 |

### SH1：Shader 描述资产与 ShaderInterface 数据模型

状态：已完成

内容：

- 定义 `.shader` schema。
- 分离定义 `ShaderGpuInterface` 与 `ShaderAuthoringMetadata`。
- 定义 Shader stage、resource、constant buffer、parameter metadata 和 canonical signature。
- 实现 descriptor 解析、路径安全和序列化测试。
- 定义 `AssetImportFingerprint`、canonical encoding 和 Library 持久化。
- 建立 `MaterialDefinition` 只读模型，但暂不接 Inspector。

验收：

- 合法 descriptor 可稳定往返。
- 非法 stage、entry、scope、重复 binding、逃逸路径被拒绝。
- 相同规范化接口生成相同 signature。
- 不同容器遍历顺序和平台运行得到相同 fingerprint/signature。
- RHI 数据结构不包含 editor metadata。

### SH2：DXC SPIR-V 编译、反射与 Shader Artifact V2

状态：已完成

内容：

- 固定 DXC SPIR-V 工具分发。
- HLSL vertex/fragment 编译。
- 收集根 descriptor、HLSL 和所有传递 include 的规范化路径与 SHA-256。
- 将 compiler identity、编译参数和完整 source inputs 纳入 Shader Import Fingerprint。
- SPIR-V validation 和 reflection。
- stage interface merge。
- Shader Artifact V2 编解码。

验收：

- 最小 HLSL 编译为包含 SPIR-V magic 的 stage artifact。
- 语法错误包含文件、行、列和 stage。
- 资源、cbuffer member、vertex input 反射准确。
- 不同 stage 接口冲突导入失败。
- 修改任意传递 include 会使 Shader Artifact 失效并重编译。
- 相同 `b/t/s` register index 造成的 SPIR-V binding 冲突在导入阶段被拒绝。

### SH3：SPIRV-Cross OpenGL GLSL 与现有渲染接通

状态：已完成

内容：

- 静态接入 SPIRV-Cross。
- 生成 desktop GLSL。
- separate texture/sampler combined mapping。
- 静态接入 glslang，并以 OpenGL semantics 验证 generated GLSL。
- ShaderProgram 从 Artifact generated GLSL 创建。
- 迁移一个 builtin Unlit Shader 作为首个纵向切片。

验收：

- HLSL -> SPIR-V -> GLSL -> OpenGL program 全链路成功。
- builtin fallback/default material 像素结果保持一致。
- runtime 删除 HLSL 后仍可从 Artifact 创建 ShaderProgram。
- Artifact 中每条 combined sampler 映射都能稳定解析为 texture、sampler 和 generated uniform。
- generated GLSL 验证失败时保留 last-good Artifact。

### SH4：OpenGL UBO 与 ShaderInterface 驱动的 BindGroup

状态：已完成。真实 OpenGL UBO、稠密 uniform block binding point、`GLsync` timeline fence、按设备对齐的 frame-local arena，以及按 ShaderInterface member offset 打包 Frame、Material、Object 常量已经接入 Forward 主路径。公共 ShaderProgram 使用后端中立的 stage binary 与完整 ShaderGpuInterface；Pipeline 创建会严格校验 binding、类型、visibility、minimum size 和完整 interface digest。OpenGL 的 uniform block binding point、texture unit 和组合采样器 uniform location 均为后端私有映射。`glFenceSync` 失败时使用同步完成退路后再发布 timeline，避免 Arena 提前复用。三个 scope 的 BindGroupLayout 均由每个 ShaderInterface 投影，材质纹理 binding 直接来自 ShaderInterface；SPIRV-Cross plain-uniform 兼容输出、OpenGL uniform name fallback、MaterialBindingSchema GPU layout 推导均已删除。

内容：

- 使用 `GLsync` 实现真实 OpenGL GPU completion fence。
- OpenGL UniformBuffer binding。
- `UniformBufferArena` 与对齐子分配。
- Frame/Material/Object parameter packing。
- Pipeline/BindGroupLayout 从 ShaderInterface 生成。
- 删除 Material 自行推导 binding 的主路径。

验收：

- ViewProjection、Transform、Color 均通过 UBO 影响真实像素。
- 错误 slot、binding、offset、range、interface signature 被拒绝。
- 一帧多对象不会为每个 draw 创建独立 OpenGL buffer。
- UBO backing buffer 在对应 GPU fence 完成前不会被覆盖或回收。
- 所需 uniform buffer binding point 超出设备上限时稳定失败。
- 现有 Forward RenderGraph 不改变 pass 业务逻辑。

### SH5：Material V2、依赖签名与导入 DAG

状态：已完成。Material Source 不再声明 `value_type` 或 `texture_slots`；Artifact 固化 Shader GPU/definition digest，fingerprint 使用 Shader interface digest。AssetImporter 通过统一依赖收集接口构建完整闭包，导入前执行拓扑排序与循环检测，Shader 导入后按 Library dependency 反向调度 Material。Artifact 使用 payload SHA-256 内容寻址，并在候选语义校验、catalog 原子保存成功后才发布内存 record；失败时保留 last-good catalog、record 和文件。

内容：

- Material source/artifact V2。
- Shader schema 驱动参数校验。
- 删除 `texture_slots` 和重复 `value_type`。
- 扩展 Import Fingerprint，使其包含 dependency GUID 和 Shader interface full digest。
- 导入 DAG 与 Shader -> Material 失效传播。
- 迁移 builtin/TestProj Material。

验收：

- Material 只提供值即可完成导入。
- 未知参数、类型不匹配和错误 Texture 引用被拒绝。
- 修改 HLSL 接口后，Material 源未修改也会自动重导。
- 修改 Shader 实现但接口未变化时，Material 不做不必要重导。
- 循环依赖在提交任何新 Artifact 前给出完整 GUID/path 链路。

### SH6：Inspector Material 参数与 Override 编辑

状态：已完成。Inspector 通过 AssetService 的 CPU-only MaterialDefinition 查询生成 Int/Float/Vector/Color/Texture2D 控件，并应用 Shader authoring metadata 中的 Range 与 Step；override、reset 和 Material 切换使用 Editor command，支持 undo/redo、scene dirty 与序列化重载。Material 切换删除不兼容 override 时会发布一次性 Workbench warning。AssetService 区分 Current、LastGoodWithFailure、Missing、Stale；启动导入或显式 Reimport 失败时，Inspector 继续显示 last-good 参数并展示最近失败诊断。

内容：

- AssetService MaterialDefinition 查询。
- Material parameter editor。
- Texture2D AssetPicker。
- override/reset、undo/redo 和 scene dirty。
- stale/missing/compile-failed 状态展示。

验收：

- 选择 Material 后显示正确参数和默认值。
- 修改 Color/Float/Texture 生成正确 override 并影响像素。
- reset 删除 override 并恢复 Material 值。
- 保存、重启和重新加载 Scene 后 override 保持。
- Material 切换不会保留类型不兼容的脏 override。

## 15. 测试策略

### 15.1 编译器测试

- 最小 vertex/fragment HLSL 编译成功。
- syntax error、missing entry、profile mismatch、include missing。
- include 路径逃逸拒绝。
- 同一 set 内 `b0/t0/s0` 映射冲突拒绝。
- 固定输入的 interface signature 稳定。
- SPIR-V 反射结果与预期 set/binding/offset 一致。

### 15.2 Artifact 与导入测试

- Shader Artifact V2 round-trip、截断、超限和版本拒绝。
- Material Artifact V2 round-trip。
- 根 source、include、compiler options、dependency signature 分别触发正确失效。
- Shader implementation-only 变化不错误重导 Material。
- Shader interface 变化重导 Material。
- 循环 import dependency 给出稳定 diagnostic。

### 15.3 OpenGL/RHI 测试

- generated GLSL 编译链接。
- UBO upload/readback 或像素验证。
- Frame/Material/Object slot 隔离。
- UBO offset alignment 和越界拒绝。
- GPU fence 完成前 UBO Arena 区间不可复用。
- Texture/Sampler combined mapping。
- Pipeline 与 BindGroupLayout signature 不匹配拒绝。

### 15.4 Editor 测试

- MaterialDefinition 到控件类型映射。
- override 新增、修改、reset。
- Texture picker 只显示 Texture2D。
- undo/redo 和 scene dirty。
- missing/stale/last-good 状态。

### 15.5 回归

每个 SH 阶段至少运行：

- `AssetImportSmoke`
- `AssetLibrarySmoke`
- `RHIResourceCreationSmoke`
- `RHICommandListBindingSmoke`
- `RenderingOperationsSmoke`
- `EditorInspectorRuntimeSmoke`
- `MaterialSerializationSmoke`

阶段完成前运行 Debug 全量构建和全部有效 smoke。

## 16. 迁移策略

1. SH0 先冻结 binding、packing、sampler、fingerprint、fence 和 last-good contract。
2. SH1-SH2 保留现有 `.glsl` Shader Artifact V1 读取，不立即破坏工程。
3. SH3 先迁移 builtin Unlit Shader，验证 HLSL 全链路；该阶段仍允许现有 `glUniform*` 提交。
4. SH4 完成真实 GPU fence 与 UBO 后迁移 Forward 主路径。
5. SH5 迁移 `Resources/BuiltinAssets` 和 `Tests/TestProj` 的 Shader/Material。
6. 所有仓库资产迁移且 smoke 通过后，在独立清理提交中删除 legacy `OpenGLShader` 文件路径构造和组合 GLSL importer 主路径。
7. 旧 Artifact 由版本变化自动重导，不写一次性二进制迁移器。

## 17. 错误处理与 Last-Good Artifact

- Shader Reimport 编译失败时，不覆盖上一个成功 Artifact 和 Library record。
- Shader V2 Artifact 通过内容寻址候选文件和 Library record 原子切换提交，禁止原地覆盖 last-good 文件。
- Import report 记录失败 source、stage、entry、文件、行和列。
- Editor 显示当前源文件编译失败，但允许场景继续使用 last-good Artifact。
- Material 发现 Shader interface full digest 失配时，不创建不兼容 bind group。
- builtin Shader 导入失败属于阻塞启动错误；项目 Shader 失败按单资产失败处理。

Last-good 提交顺序固定为：

1. 在内存中完成 Shader Artifact 编码。
2. 使用与 runtime 相同的 limits 立即解码并校验 kind、version、digest、stage、SPIR-V magic、generated GLSL 和 resource map。
3. 计算完整 Artifact payload SHA-256，并使用 `Artifacts/<guid>.<digest>.huashader` 作为候选路径；通过现有 `WriteAssetBinaryFileAtomically()` 写入候选文件。
4. 从候选文件读取并再次校验 header、payload digest 和 Shader Artifact contract。
5. 暂存指向候选路径的新 Library record，并原子保存 Library catalog；保存失败时回滚内存 record、删除候选文件，last-good record 和文件保持不变。
6. Catalog 保存成功后发布新内存 record、失效 runtime cache，并把旧 Artifact 文件加入延迟垃圾回收；旧文件删除失败不影响新 Artifact 可用性。

现有原子写 helper 已满足同目录临时文件和 Windows `MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH`；Shader V2 复用该 helper写候选文件和 catalog，不另建非原子写入路径。Library 打开时只信任 catalog 指向的 Artifact；未被引用的内容寻址文件可安全清理。

## 18. 完成定义

满足以下条件视为本 Spec 完成：

- 仓库 builtin 与 TestProj Shader 以 HLSL `.shader` 资产存在。
- Shader 在导入阶段通过 DXC 生成 SPIR-V，通过 SPIRV-Cross 生成 OpenGL GLSL。
- OpenGL runtime 不读取 HLSL/Shader descriptor。
- ShaderInterface 是 Pipeline 和 BindGroupLayout 的唯一 GPU 接口来源。
- Frame、Material、Object 常量通过真实 UBO 提交。
- Material 不再保存 binding、texture slot 和重复参数类型声明。
- Shader interface 变化能正确使 Material 失效，implementation-only 变化不会引发无关 Material 重导。
- Inspector 能编辑实体 Material override，支持 undo/redo、保存和重载。
- Debug 全量构建及全部有效 smoke 通过。
- D3D12 仍可继续后置，且未来只需为同一 HLSL 增加 DXIL Artifact 和 backend consumption，不需要再次重构 Material/ShaderInterface。

## 19. 风险与控制

### 19.1 HLSL、SPIR-V 与 GLSL 语义差异

控制：首版限制 Shader 类型和语法子集；所有 builtin Shader 进入像素级 smoke；生成 GLSL 在导入阶段完成 OpenGL 编译验证。

### 19.2 常量缓冲布局差异

控制：首版限制参数类型，ShaderInterface 保存明确 offset/size，CPU 只按反射结果打包；未来 DXIL 接入必须进行 interface compatibility 测试，不假定 SPIR-V 与 DXIL packing 天然一致。

### 19.3 Texture/Sampler 组合差异

控制：公共接口保持分离资源，OpenGL 映射表负责组合；首版 Material 不保存 Sampler，由引擎提供 canonical default sampler；Material 不感知 texture unit。

### 19.4 工具链不可复现

控制：固定 DXC、SPIRV-Cross 和 glslang 版本，将版本和编译参数纳入 fingerprint；不依赖 PATH、Vulkan SDK 或用户机器环境。

### 19.5 一次性重构范围过大

控制：严格按 SH0-SH6 纵向推进，每阶段独立提交和测试；在 SH4 前保留 GLSL/uniform 兼容路径，在 SH5 迁移完成后再清理 legacy。

### 19.6 SPIR-V descriptor binding 冲突

控制：HLSL 同时显式声明 DXIL register 与 `vk::binding`；同一 set 内所有资源 binding 唯一；descriptor 解析、stage merge 和 Artifact decode 三层都执行冲突校验。

### 19.7 UBO 区间过早复用

控制：OpenGL 使用真实 `GLsync` 表达 GPU completion；Arena 只回收 fence 已完成的区间；容量不足时扩容或稳定失败，不覆盖仍被 GPU 引用的数据。

## 20. 代码勘探摘要

- `ShaderArtifactData` 当前仅包含 `VertexSource` 和 `FragmentSource`，Artifact 版本为 1。
- `GlslShaderImporter` 当前解析单文件 `#type vertex/fragment` 组合 GLSL，不执行离线 GPU 编译和资源反射。
- `MaterialSourceData` 已使用 `ShaderGuid`，`MaterialAssetImporter` 已校验 Shader GUID 并登记 artifact dependency。
- `AssetResolver::ResolveShader()` 当前从 Artifact 解码 GLSL，再调用 `ShaderProgramLoader::CreateFromSource()` 创建运行时 OpenGL program。
- `Material::GetBindingSchema()` 当前按参数名排序并顺序分配 binding，这是应由 ShaderInterface 替代的核心临时逻辑。
- `BindGroupScope` 已定义 Frame、Material、Object；Forward pass 已固定使用 slot 0、1、2。
- RHI 已支持 `UniformBuffer`、buffer offset/size、buffer upload 和 pipeline layout signature 校验。
- OpenGL `SetBindGroup()` 当前处理 scalar/vector/matrix、Texture、TextureView 和 Sampler，但未处理 `Ref<GpuBuffer>`，因此 UniformBuffer contract 尚未进入真实渲染主路径。
- OpenGL queue 当前 fence 在 CPU command replay 后立即 signal，不代表 GPU completion，不能直接用于 UBO Arena 回收。
- Inspector 当前通过 Runtime Reflection 编辑普通字段，并为 Mesh/Material AssetRef 提供 picker；`MaterialOverrideSet` 已被识别为 Object，但没有专用参数编辑器和 MaterialDefinition 数据源。
- Asset Library 当前使用根 source SHA-256 判断新鲜度，尚未包含 Shader include 和 dependency artifact signature。

## 21. 参考资料

- Microsoft DirectXShaderCompiler SPIR-V 文档：<https://github.com/microsoft/DirectXShaderCompiler/blob/main/docs/SPIR-V.rst>
- Microsoft DirectXShaderCompiler：<https://github.com/microsoft/DirectXShaderCompiler>
- Khronos SPIRV-Cross：<https://github.com/KhronosGroup/SPIRV-Cross>
- Khronos OpenGL `ARB_gl_spirv`：<https://registry.khronos.org/OpenGL/extensions/ARB/ARB_gl_spirv.txt>
- Khronos glslang：<https://github.com/KhronosGroup/glslang>
