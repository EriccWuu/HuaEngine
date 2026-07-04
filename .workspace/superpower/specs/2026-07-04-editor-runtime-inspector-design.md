# Editor Runtime Inspector 重构设计

## 背景

当前 Inspector 面板的组件内容显示仍然走旧路径：

- `InspectorPanel` 调用 `ComponentEditorRegistry::DrawComponents()`。
- `ComponentEditorRegistry` 遍历手写 `REGISTER_COMPONENT_EDITOR(...)` 注册的组件类型。
- 每个组件通过 `world.TryGetComponent<T>()` 获取实例。
- 字段绘制通过 `DrawComponentEditor<T>() -> Refl::reflect<T>().visit_fields(...)` 静态反射完成。

这与 P3 后的反射主路径不一致。组件序列化、组件注册和 runtime metadata 已经统一到 `RuntimeTypeDescriptor` / `RuntimeFieldDescriptor`，但 Editor Inspector 仍依赖手写组件 editor 注册和静态反射字段遍历。

本设计目标是把 Inspector 主链路迁移到 runtime reflection facade，手写 editor 只作为 override fallback。

## 目标

1. Inspector 显示实体真实拥有的 runtime-registered components，而不是只显示手写注册列表里的组件。
2. Inspector 字段显示和编辑走 `RuntimeTypeDescriptor` / `RuntimeFieldDescriptor`。
3. `TransformComponent` 的 `Position` / `Rotation` / `Scale` 通过 runtime field editor 可编辑。
4. `CameraComponent` 的 `bool` 字段通过 runtime field editor 可编辑。
5. 手写 component editor 保留为特殊组件 override，不再是主路径。
6. Add Component 来源逐步改为 `ComponentRegistry::GetAll()`。
7. Remove Component 在现有 command/enum 能力范围内保持行为不回归；runtime-only 组件短期可以禁用 remove。

## 非目标

- 不在本阶段实现完整复杂类型编辑器。
- 不在本阶段重写 Material inspector 的专用 UI。
- 不在本阶段彻底删除 `ComponentEditorRegistry` 或 `DrawComponentEditor<T>()`。
- 不在本阶段把 add/remove command system 完全改成 runtime component operation。
- 不在本阶段改变 scene serialization 格式。

## 推荐架构

Inspector 主链路改为：

```text
Selection
  -> Entity.ListComponentTypes()
  -> ComponentRegistry.FindByTypeId(typeId)
  -> ComponentMetadata.RuntimeType
  -> RuntimeTypeDescriptor.Fields
  -> RuntimeFieldEditor
```

`ComponentEditorRegistry` 降级为 override registry：

```text
Runtime component found
  -> has explicit editor override?
      -> use override editor
      -> else use generic runtime field editor
```

命名上可以选择保留 `ComponentEditorRegistry`，但语义应调整为“override registry”。如果改名成本较高，可以先保留类名并在接口命名上体现 override，例如 `FindOverride()` / `DrawOverride()`。

## 组件显示流程

### 单选实体

1. Inspector 从 selection 解析当前 entity。
2. 使用 `entity.ListComponentTypes()` 获取实体实际拥有的组件类型。
3. 对每个 type id，通过 editor 持有的 `ComponentRegistry` 查找 metadata。
4. 若找不到 metadata，则显示 fallback header，例如 `Unknown Component <id>`，内容只读。
5. 若 metadata 存在：
   - header 名称优先使用 `RuntimeTypeDescriptor::DisplayName`。
   - DisplayName 为空时使用 `ComponentMetadata::DisplayName`。
   - 再为空时使用 `RuntimeTypeDescriptor::Name` / `ComponentMetadata::TypeName`。
6. header 展开后：
   - 如果 override registry 有该类型专用 editor，调用 override。
   - 否则调用 generic runtime inspector。

### 多选实体

本阶段保持现状：只显示 summary，不做批量字段编辑。

## Runtime 字段编辑器

新增 runtime field editor 层，输入：

```cpp
bool DrawRuntimeFieldEditor(
    const Refl::RuntimeFieldDescriptor& field,
    void* component);
```

执行逻辑：

1. 跳过非 `Serializable` 或无 `GetMutable` 的字段。
2. 使用 `field.GetMutable(component)` 获取字段地址。
3. 按 `field.Type` 分派到具体 ImGui 控件。
4. 返回是否发生修改。

第一阶段支持类型：

| Type 字符串 | UI |
| --- | --- |
| `bool` | `ImGui::Checkbox` |
| `int8_t` / `int16_t` / `int32_t` / `int64_t` / `int` | `ImGui::DragScalar` 或 `InputScalar` |
| `uint8_t` / `uint16_t` / `uint32_t` / `uint64_t` / `unsigned int` | `ImGui::DragScalar` 或 `InputScalar` |
| `float` | `ImGui::DragFloat` |
| `double` | `ImGui::InputDouble` 或 `DragScalar` |
| `std::string` | `ImGui::InputText`，使用临时 buffer 回写 |
| `glm::vec2` | `ImGui::DragFloat2` |
| `glm::vec3` | `ImGui::DragFloat3` |
| `glm::vec4` | `ImGui::DragFloat4` |

复杂类型处理：

- `Ref<...>`：第一阶段显示 disabled text，例如 `Unsupported runtime field: Ref<...>`。
- nested struct：第一阶段只读显示类型名，不展开。
- enum：第一阶段按 underlying type 不自动处理，后续可通过 runtime enum metadata 扩展。

## Add Component

Add Component 窗口来源从 `GetEditorInspectableComponents()` 逐步改为 `ComponentRegistry::GetAll()`。

显示规则：

1. 遍历 registry metadata。
2. 若 entity 已拥有该 type id，显示 `(Already Added)`。
3. 若 metadata 缺少 `ConstructDefault` 或 `AddCopyToWorld`，显示 disabled。
4. 已有 `EditorInspectableComponent` 映射的组件继续走现有 add callback。
5. 没有映射的 runtime-only 组件短期显示 disabled，原因标注为 `Runtime add command not available`。

这样本阶段能先统一展示来源，同时不扩大 command system 改造范围。

## Remove Component

Remove Component 继续通过现有 `EditorInspectableComponent` callback。

规则：

1. 有 enum 映射且 `CanRemoveInspectableComponent()` 允许时，显示 enabled remove。
2. 没有 enum 映射的 runtime-only 组件，context menu 中显示 disabled remove。
3. 不在本阶段新增 generic remove-by-type-id command。

后续可在 Editor command system 中增加 runtime component remove operation，再解除该限制。

## 数据依赖

Inspector 需要访问一个核心组件 registry。

推荐做法：

- 在 `InspectorPanel` 内部持有 `ComponentRegistry m_ComponentRegistry`。
- 构造或初始化时调用 `RegisterCoreComponents(m_ComponentRegistry)`。
- Add/Remove/Draw 都使用同一个 registry。

备选做法是由 `EditorLayer` 或 `EditorInteractionHost` 提供 registry 指针，但当前系统里 scene serializer 已多处局部构造 registry，短期在 Inspector 内持有 registry 更低风险。

## 错误处理

- metadata 缺失：显示只读 fallback，不崩溃。
- component pointer 缺失：跳过该组件。
- runtime descriptor 缺失：显示 header + disabled text。
- field accessor 缺失：显示 disabled field。
- unsupported type：显示 disabled type text。
- editor override 抛异常不是当前 C++ 代码风格，本阶段不加异常边界。

## 测试策略

新增或更新 smoke tests：

1. `EditorInspectorRuntimeSmoke`
   - 构造 world/entity。
   - 注册 core components。
   - 使用 runtime inspector helper 枚举 entity components。
   - 验证 `TransformComponent` 可通过 runtime metadata 找到 `Position/Rotation/Scale`。
   - 验证 `CameraComponent` 的 `Primary` / `FixedAspectRatio` bool 字段可被识别为 editable。

2. 轻量 helper 测试
   - 不直接依赖真实 ImGui frame 的情况下，测试 type 分派函数，例如 `IsRuntimeFieldEditableType(field.Type)`。
   - 如果现有测试框架不适合链接 Editor，可以先把 pure helper 放在 Editor 可测模块或用 build-only smoke 覆盖。

3. 回归搜索
   - Inspector 主路径不再调用 `Refl::reflect<T>()`。
   - `ComponentEditor.h` 可以保留旧实现，但不应被 Inspector 主路径直接使用。

手动验证：

- 启动 Editor。
- 选中 entity。
- Inspector 显示 Transform/Camera/Mesh/Material 等实体实际组件。
- 修改 Transform Position，scene document 变 dirty。
- 修改 Camera bool 字段，UI 状态同步。
- Unsupported complex field 显示为只读/disabled，而不是崩溃。

## 风险

- 当前 `RuntimeFieldDescriptor::Type` 是字符串，字段 editor 分派依赖字符串匹配。短期可接受，后续应升级为 runtime type enum/type id。
- runtime-only add/remove 仍受 Editor command enum 限制。显示来源统一后，可能出现“能看到但不能加/删”的组件，需要明确 disabled 状态。
- `std::string` 输入需要 buffer 管理，不能直接把 `std::string::data()` 作为长期可写 buffer。
- `MaterialComponent` 等复杂类型若没有 override，会显示降级 UI；需要通过 override 保持体验。

## 验收标准

- Inspector 主组件显示路径从 entity runtime component list 驱动。
- `TransformComponent` 不依赖 `REGISTER_COMPONENT_EDITOR(TransformComponent)` 也能显示字段。
- `TransformComponent` 的 `Position` / `Rotation` / `Scale` 可编辑。
- `CameraComponent` 的 `Primary` / `FixedAspectRatio` 可编辑。
- Inspector 主路径不再调用 `Refl::reflect<T>()`。
- `ComponentEditorRegistry` 仅作为 override/fallback 使用。
- Add Component 窗口使用 `ComponentRegistry::GetAll()` 作为候选来源。
- 现有 remove 行为对已支持组件不回归。
- Unsupported 字段类型不会崩溃，显示明确 disabled 文案。
