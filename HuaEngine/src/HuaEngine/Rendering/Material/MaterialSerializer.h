#pragma once

#include "MaterialCore.h"
#include "HuaEngine/Serialization/Serialization.h"

namespace HE::Rendering {

    // 材质参数值序列化
    class MaterialParameterSerializer {
    public:
        static void Serialize(HE::Serialization::SerializationBackend& backend, const std::string& name, const MaterialParameterValue& value);
        static bool Deserialize(HE::Serialization::SerializationBackend& backend, const std::string& name, MaterialParameterValue& value, MaterialParameterType type);
        
        // 辅助函数：从类型名称获取类型枚举
        static MaterialParameterType StringToParameterType(const std::string& typeStr);
        static std::string ParameterTypeToString(MaterialParameterType type);
    };

    // 材质类型序列化
    inline std::string MaterialTypeToString(MaterialType type) {
        switch (type) {
            case MaterialType::Standard: return "Standard";
            case MaterialType::Unlit: return "Unlit";
            case MaterialType::Custom: return "Custom";
            default: return "Unknown";
        }
    }

    inline MaterialType StringToMaterialType(const std::string& typeStr) {
        if (typeStr == "Standard") return MaterialType::Standard;
        if (typeStr == "Unlit") return MaterialType::Unlit;
        if (typeStr == "Custom") return MaterialType::Custom;
        return MaterialType::Custom; // 默认为自定义
    }

    inline bool SaveMaterial(Material* material, const std::string& filename, HE::Serialization::SerializationFormat format = HE::Serialization::SerializationFormat::JSON) {
        return SERIALIZE_TO_FILE(*material, filename, format);
    }

    inline bool LoadMaterial(const std::string& filename, Material* material, HE::Serialization::SerializationFormat format = HE::Serialization::SerializationFormat::JSON) {
        return DESERIALIZE_FROM_FILE(filename, *material, format);
    }

} // namespace HE::Rendering

namespace HE::Serialization {

    // 材质参数序列化 (使用参数名作为对象键)
    // 序列化格式: "paramName": { "type": "Float", "value": 0.5 }
    template<>
    struct Serializer<Rendering::MaterialParameter> {
        // 序列化单个参数 (参数名作为对象键)
        static void Serialize(SerializationBackend& backend, const std::string& name, const Rendering::MaterialParameter& param) {
            // name 参数就是参数名，用作对象键
            backend.BeginObject(name);
            backend.Serialize("type", Rendering::MaterialParameterSerializer::ParameterTypeToString(param.Type));
            Rendering::MaterialParameterSerializer::Serialize(backend, "value", param.Value);
            backend.EndObject();
        }

        // 反序列化单个参数 (需要外部提供参数名)
        static bool Deserialize(SerializationBackend& backend, const std::string& name, Rendering::MaterialParameter& param) {
            backend.BeginObject(name);

            if (!(backend.HasField("type") && backend.HasField("value"))) {
                backend.EndObject();
                return false;
            }

            // 参数名由外部传入
            param.Name = name;

            std::string typeStr;
            backend.Deserialize("type", typeStr);
            param.Type = Rendering::MaterialParameterSerializer::StringToParameterType(typeStr);

            Rendering::MaterialParameterSerializer::Deserialize(backend, "value", param.Value, param.Type);

            backend.EndObject();
            return true;
        }
    };

    // 材质基类序列化
    template<>
    struct Serializer<Rendering::Material> {
        static void Serialize(SerializationBackend& backend, const std::string& name, const Rendering::Material& material) {
            if (!name.empty())
                backend.BeginObject(name);

            // 基本属性
            backend.Serialize("name", material.GetName());
            backend.Serialize("type", Rendering::MaterialTypeToString(material.GetType()));

            // Shader 路径
            std::string shaderPath = "";
            if (material.GetShader()) {
                shaderPath = material.GetShader()->GetPath();
            }
            backend.Serialize("shader_path", shaderPath);

            // 参数列表 (对象格式: "paramName": { "type": "...", "value": ... })
            const auto& parameters = material.GetParameters();
            backend.BeginObject("parameters");
            for (const auto& [paramName, param] : parameters) {
                Serializer<Rendering::MaterialParameter>::Serialize(backend, paramName, param);
            }
            backend.EndObject();

            // 纹理槽信息
            const auto& textureSlots = material.GetTextureSlots();
            if (!textureSlots.empty()) {
                backend.BeginObject("texture_slots");
                for (const auto& [slotName, slotIndex] : textureSlots) {
                    backend.Serialize(slotName, static_cast<int>(slotIndex));
                }
                backend.EndObject();
            }

            if (!name.empty())
                backend.EndObject();
        }

        static bool Deserialize(SerializationBackend& backend, const std::string& name, Rendering::Material& material) {
            if (!name.empty())
                backend.BeginObject(name);

            // 检查必要字段
            if (!(backend.HasField("type") &&
                backend.HasField("name") &&
                backend.HasField("parameters"))) {
                return false;
            }

            std::string shaderPath;
            if (backend.HasField("shader_path")) {
                backend.Deserialize("shader_path", shaderPath);
            }

            std::string typeStr;
            backend.Deserialize("type", typeStr);
            material.SetType(Rendering::StringToMaterialType(typeStr));

            std::string matName;
            backend.Deserialize("name", matName);
            material.SetName(matName);

            // 根据 shaderPath 加载 Shader
            if (!shaderPath.empty()) {
                auto shader = Rendering::Shader::CreateFromFile(shaderPath);
                material.SetShader(shader);
            }

            // 参数列表 (对象格式，使用 ForEachField 遍历)
            backend.BeginObject("parameters");
            backend.ForEachField([&](const std::string& paramName) {
                Rendering::MaterialParameter param;
                // ForEachField 已将上下文切换到 paramName 对应的值节点
                if (backend.HasField("type") && backend.HasField("value")) {
                    param.Name = paramName;

                    std::string typeStr;
                    backend.Deserialize("type", typeStr);
                    param.Type = Rendering::MaterialParameterSerializer::StringToParameterType(typeStr);

                    Rendering::MaterialParameterSerializer::Deserialize(backend, "value", param.Value, param.Type);

                    material.AddParameter(param);
                }
            });
            backend.EndObject();

            // 纹理槽信息 (使用 ForEachField 遍历)
            if (backend.HasField("texture_slots")) {
                backend.BeginObject("texture_slots");
                backend.ForEachField([&](const std::string& slotName) {
                    int slotIndex = 0;
                    backend.Deserialize("", slotIndex);
                    material.SetTextureSlot(slotName, static_cast<uint32_t>(slotIndex));
                });
                backend.EndObject();
            }

            if (!name.empty())
                backend.EndObject();

            return true;
        }
    };

    // 材质实例序列化
    template<>
    struct Serializer<Rendering::MaterialInstance> {
        static void Serialize(SerializationBackend& backend, const std::string& name, const Rendering::MaterialInstance& instance) {
            if (!name.empty())
                backend.BeginObject(name);

            // 基础材质名称
            backend.Serialize("base_material_name", instance.GetBaseMaterial()->GetName());

            // 参数覆盖 (对象格式: "paramName": { "type": "...", "value": ... })
            const auto& overrideParams = instance.GetParameterOverrides();
            backend.BeginObject("parameter_overrides");
            for (const auto& [paramName, param] : overrideParams) {
                Serializer<Rendering::MaterialParameter>::Serialize(backend, paramName, param);
            }
            backend.EndObject();

            if (!name.empty())
                backend.EndObject();
        }

        static bool Deserialize(SerializationBackend& backend, const std::string& name, Rendering::MaterialInstance& instance) {
            if (!name.empty())
                backend.BeginObject(name);

            if (!(backend.HasField("base_material_name") &&
                backend.HasField("parameter_overrides")))
                return false;

            std::string baseMaterialName;
            backend.Deserialize("base_material_name", baseMaterialName);

            // 清除现有的参数覆盖
            instance.ClearParameterOverrides();

            // 参数覆盖 (对象格式，使用 ForEachField 遍历)
            backend.BeginObject("parameter_overrides");
            backend.ForEachField([&](const std::string& paramName) {
                // ForEachField 已将上下文切换到 paramName 对应的值节点
                if (backend.HasField("type") && backend.HasField("value")) {
                    std::string typeStr;
                    backend.Deserialize("type", typeStr);
                    auto type = Rendering::MaterialParameterSerializer::StringToParameterType(typeStr);

                    Rendering::MaterialParameterValue value;
                    Rendering::MaterialParameterSerializer::Deserialize(backend, "value", value, type);

                    instance.SetParameter(paramName, value);
                }
            });
            backend.EndObject();

            if (!name.empty())
                backend.EndObject();

            return true;
        }
    };

} // namespace HE::Serialization
