# HuaEngine 序列化系统

这是一个完整的、可扩展的序列化系统，支持多种后端格式。

## 设计特性

### 1. 抽象接口层
- `SerializationBackend` - 统一的序列化接口
- 支持对象、数组和基本数据类型
- 完整的查询操作支持

### 2. 多格式支持
- **JSON Backend** - 内置的JSON序列化支持（无外部依赖）
- **YAML Backend** - 可选的YAML支持（需要yaml-cpp库）
- **Binary Backend** - 未来可扩展的二进制格式

### 3. 自动反射序列化
- 基于现有反射系统的自动序列化
- 支持基本类型、GLM向量、自定义结构体
- 数组和容器支持

### 4. 场景序列化
- 完整的场景保存/加载
- 实体和组件系统集成
- 支持多种文件格式

## 使用方法

### 1. 初始化系统

```cpp
#include "HuaEngine/Serialization/SerializationCore.h"

// 在引擎启动时调用
HE::InitializeSerialization();
```

### 2. 简单对象序列化

```cpp
// 序列化到JSON字符串
TransformComponent transform;
transform.Position = {1.0f, 2.0f, 3.0f};
std::string json = HE::ToJson(transform);

// 从JSON字符串反序列化
TransformComponent loaded;
HE::FromJson(json, loaded);

// 文件操作
HE::SaveAsJson(transform, "transform.json");
HE::LoadFromJson("transform.json", loaded);
```

### 3. 场景序列化

```cpp
// 保存场景
HE::SaveScene(&scene, "my_scene.json");

// 加载场景
HE::LoadScene(&scene, "my_scene.json");

// 支持不同格式
HE::SaveScene(&scene, "my_scene.yaml", SerializationFormat::YAML);
```

### 4. 自定义类型序列化

对于现有反射支持的类型，序列化是自动的。对于自定义类型：

```cpp
// 使用反射宏
srefl_class(MyComponent,
    fields(
        field(myField1),
        field(myField2)
    )
)

// 或者自定义序列化器
template<>
struct Serializer<MyCustomType> {
    static void Serialize(SerializationBackend& backend, const std::string& name, const MyCustomType& obj) {
        backend.BeginObject(name);
        backend.Serialize("field1", obj.field1);
        backend.Serialize("field2", obj.field2);
        backend.EndObject();
    }

    static bool Deserialize(SerializationBackend& backend, const std::string& name, MyCustomType& obj) {
        if (!backend.HasField(name)) return false;
        
        backend.BeginObject(name);
        backend.Deserialize("field1", obj.field1);
        backend.Deserialize("field2", obj.field2);
        backend.EndObject();
        return true;
    }
};
```

### 5. 底层API使用

```cpp
// 创建特定后端
auto backend = SerializationManager::Instance().CreateBackend(SerializationFormat::JSON);

// 手动序列化
backend->BeginObject();
backend->Serialize("name", "Player");
backend->Serialize("level", 10);
backend->BeginArray("inventory", 3);
for (int i = 0; i < 3; ++i) {
    backend->BeginArrayElement(i);
    backend->Serialize("", items[i]);
    backend->EndArrayElement();
}
backend->EndArray();
backend->EndObject();

// 保存结果
std::string result = backend->SaveToString();
```

## 支持的数据类型

### 基本类型
- `bool`
- 整数类型：`int8_t`, `int16_t`, `int32_t`, `int64_t`, `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`
- 浮点类型：`float`, `double`
- `std::string`

### GLM类型
- `glm::vec3`, `glm::vec4`
- `glm::mat4`

### 容器类型
- `std::vector<T>`

### 自定义类型
- 任何具有反射信息的结构体/类
- 可以通过自定义序列化器扩展

## 文件结构

```
HuaEngine/Serialization/
├── Serialization.h              # 核心接口和类型定义
├── Serialization.cpp            # 初始化代码
├── SerializationManager.h       # 序列化管理器
├── SerializationManager.cpp     # 管理器实现
├── ReflectionSerializer.h       # 基于反射的序列化器
├── JsonSerializationBackend.h   # JSON后端接口
├── JsonSerializationBackend.cpp # JSON后端实现
├── YamlSerializationBackend.h   # YAML后端接口（可选）
├── SceneSerializer.h            # 场景序列化器
├── SceneSerializer.cpp          # 场景序列化实现
├── SerializationCore.h          # 便捷接口
└── SerializationExamples.h      # 使用示例
```

## 扩展新格式

要添加新的序列化格式：

1. 继承 `SerializationBackend` 接口
2. 实现所有虚函数
3. 在启动时注册到 `SerializationManager`

```cpp
class MySerializationBackend : public SerializationBackend {
    // 实现所有接口方法
};

// 注册
SerializationManager::Instance().RegisterBackend(
    SerializationFormat::MY_FORMAT,
    []() -> std::unique_ptr<SerializationBackend> {
        return std::make_unique<MySerializationBackend>();
    }
);
```

## 性能注意事项

1. **JSON解析器** - 当前使用简单的字符串解析器，适合中小型数据
2. **内存使用** - 对象在序列化时会构建完整的内存表示
3. **反射开销** - 反射序列化有轻微的运行时开销

## 未来改进

1. **性能优化** - 可集成高性能JSON库（如nlohmann/json, rapidjson）
2. **二进制格式** - 添加高效的二进制序列化支持
3. **版本控制** - 添加序列化版本管理
4. **增量序列化** - 支持差量更新
5. **压缩支持** - 集成压缩算法

## 错误处理

系统使用异常和返回值两种错误处理方式：
- 文件I/O错误会抛出异常
- 数据类型不匹配返回false
- 可以通过日志系统查看详细错误信息
