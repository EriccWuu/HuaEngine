#pragma once

#include "MaterialCore.h"
#include "HuaEngine/Serialization/Serialization.h"

namespace HE::Rendering {

    // 材质参数值序列化
    class MaterialParameterSerializer {
    public:
        static void Serialize(SerializationBackend& backend, const std::string& name, const MaterialParameterValue& value);
        static bool Deserialize(SerializationBackend& backend, const std::string& name, MaterialParameterValue& value, MaterialParameterType type);
        
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

    inline bool SaveMaterial(Material* material, const std::string& filename, SerializationFormat format = SerializationFormat::JSON) {
        return SERIALIZE_TO_FILE(*material, filename, format);
    }

    inline bool LoadMaterial(const std::string& filename, Material* material, SerializationFormat format = SerializationFormat::JSON) {
        return DESERIALIZE_FROM_FILE(filename, *material, format);
    }

} // namespace HE::Rendering

namespace HE {

    // 材质参数序列化
    template<>
    struct Serializer<Rendering::MaterialParameter> {
        static void Serialize(SerializationBackend& backend, const std::string& name, const Rendering::MaterialParameter& param) {
            backend.BeginObject(name);
            backend.Serialize("name", param.Name);
            backend.Serialize("type", Rendering::MaterialParameterSerializer::ParameterTypeToString(param.Type));
            backend.Serialize("is_texture", param.IsTexture);
            
            // 序列化默认值
            Rendering::MaterialParameterSerializer::Serialize(backend, "default_value", param.DefaultValue);
            
            backend.EndObject();
        }

        static bool Deserialize(SerializationBackend& backend, const std::string& name, Rendering::MaterialParameter& param) {            
            backend.BeginObject(name);
            
            // 在对象内部检查字段
            if (!(backend.HasField("name") &&
                backend.HasField("type") &&
                backend.HasField("is_texture") &&
                backend.HasField("default_value"))) {
                backend.EndObject();
                return false;
            }
                
            backend.Deserialize("name", param.Name);
                
            std::string typeStr;
            backend.Deserialize("type", typeStr);
            param.Type = Rendering::MaterialParameterSerializer::StringToParameterType(typeStr);
                
            backend.Deserialize("is_texture", param.IsTexture);
                
            // 反序列化默认值
            Rendering::MaterialParameterSerializer::Deserialize(backend, "default_value", param.DefaultValue, param.Type);
                
            backend.EndObject();
            return true;
        }
    };

    // 材质基类序列化
    template<>
    struct Serializer<Rendering::Material> {
        static void Serialize(SerializationBackend& backend, const std::string& name, const Rendering::Material& material) {
            // 基本属性
            backend.Serialize("name", material.GetName());
            backend.Serialize("type", Rendering::MaterialTypeToString(material.GetType()));
            
            // Shader 路径 (简化处理，只保存路径)
            // TODO: 需要 Shader 类支持路径存储才能完整实现
            std::string shaderPath = ""; // 暂时为空，等待 Shader 类扩展
            if (material.GetShader()) {
                // 当 Shader 支持路径存储时，可以这样获取：
                shaderPath = material.GetShader()->GetPath();
            }
            backend.Serialize("shader_path", shaderPath);
            
            // 参数列表
            const auto& parameters = material.GetParameters();
            backend.BeginArray("parameters", parameters.size());
            
            size_t index = 0;
            for (const auto& [paramName, param] : parameters) {
                backend.BeginArrayElement(index++);
                SerializeValue(backend, "", param);
                backend.EndArrayElement();
            }
            backend.EndArray();
            
            // 纹理槽信息
            const auto& textureSlots = material.GetTextureSlots();
            if (!textureSlots.empty()) {
                backend.BeginObject("texture_slots");
                for (const auto& [slotName, slotIndex] : textureSlots) {
                    backend.Serialize(slotName, static_cast<int>(slotIndex));
                }
                backend.EndObject();
            }
        }

        static bool Deserialize(SerializationBackend& backend, const std::string& name, Rendering::Material& material) {    
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

            // 参数列表
            // 先获取数组大小，再进入数组
            size_t paramCount = backend.GetArraySize("parameters");
            backend.BeginArray("parameters");
            for (size_t i = 0; i < paramCount; ++i) {
                backend.BeginArrayElement(i);
                Rendering::MaterialParameter param;
                DeserializeValue(backend, "", param);
                material.AddParameter(param);
                backend.EndArrayElement();
            }
            backend.EndArray();

            // 纹理槽信息
            if (backend.HasField("texture_slots")) {
                backend.BeginObject("texture_slots");
                // 注意：具体的字段名需要在运行时确定，这里提供一个示例
                // 实际实现可能需要遍历对象的所有字段
                // 暂时跳过具体实现，留待后续完善
                backend.EndObject();
            }

            return true;
        }
    };

    // 材质实例序列化
    template<>
    struct Serializer<Rendering::MaterialInstance> {
        static void Serialize(SerializationBackend& backend, const std::string& name, const Rendering::MaterialInstance& instance) {
            backend.BeginObject(name);
            
            // 基础材质名称
            backend.Serialize("base_material_name", instance.GetBaseMaterial()->GetName());
            
            // 参数覆盖
            backend.BeginArray("parameter_overrides");
            const auto& overrideParams = instance.GetParameterOverrides();

            size_t index = 0;
            for (const auto& [paramName, param] : overrideParams) {
                backend.BeginArrayElement(index++);
                backend.BeginObject();
                Rendering::MaterialParameterSerializer::Serialize(backend, paramName, param);
                backend.EndObject();
                backend.EndArrayElement();
            }
            backend.EndArray();
            
            backend.EndObject();
        }

        static bool Deserialize(SerializationBackend& backend, const std::string& name, Rendering::MaterialInstance& instance) {
            if (!(backend.HasField("base_material_name") &&
                backend.HasField("parameter_overrides")))
                return false;
            
            // 注意：MaterialInstance 的反序列化需要先有基础材质
            // 通常在创建时就指定基础材质，这里主要恢复参数覆盖
                
            std::string baseMaterialName;
            backend.Deserialize("base_material_name", baseMaterialName);
                
            // 参数覆盖
            // 先获取数组大小，再进入数组
            size_t paramCount = backend.GetArraySize("parameter_overrides");
            backend.BeginArray("parameter_overrides");

            // 清除现有的参数覆盖
            instance.ClearParameterOverrides();

            for (size_t i = 0; i < paramCount; ++i) {
                backend.BeginArrayElement(i);
                backend.BeginObject("");
                        
                // 获取参数名和对应的基础材质参数信息
                auto baseMaterial = instance.GetBaseMaterial();
                if (baseMaterial) {
                    const auto& parameters = baseMaterial->GetParameters();
                            
                    // 遍历所有可能的参数名
                    for (const auto& [paramName, baseParam] : parameters) {
                        if (backend.HasField(paramName)) {
                            Rendering::MaterialParameterValue value;
                            if (Rendering::MaterialParameterSerializer::Deserialize(backend, paramName, value, baseParam.Type)) {
                                instance.SetParameter(paramName, value);
                            }
                        }
                    }
                }
                        
                backend.EndObject();
                backend.EndArrayElement();
            }
            backend.EndArray();
                
            return true;
        }
    };

} // namespace HE
