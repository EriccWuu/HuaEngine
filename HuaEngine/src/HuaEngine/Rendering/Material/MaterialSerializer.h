#pragma once

#include "MaterialCore.h"
#include "MaterialLibrary.h"
#include "HuaEngine/Serialization/Serialization.h"

namespace HE::Rendering {

    // Material parameter value serialization
    class MaterialParameterSerializer {
    public:
        static void Serialize(HE::Serialization::SerializationBackend& backend, const std::string& name, const MaterialParameterValue& value);
        static bool Deserialize(HE::Serialization::SerializationBackend& backend, const std::string& name, MaterialParameterValue& value, MaterialParameterType type);

        // Helper: convert between type string and enum
        static MaterialParameterType StringToParameterType(const std::string& typeStr);
        static std::string ParameterTypeToString(MaterialParameterType type);
    };

    // Material type serialization
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
        return MaterialType::Custom; // Default to custom
    }

} // namespace HE::Rendering

namespace HE::Serialization {

    // Material parameter serialization (uses parameter name as object key)
    // Format: "paramName": { "value_type": "Float", "value": 0.5 }
    template<>
    struct Serializer<Rendering::MaterialParameter> {
        // Serialize single parameter (parameter name as object key)
        static void Serialize(SerializationBackend& backend, const std::string& name, const Rendering::MaterialParameter& param) {
            backend.BeginObject(name);
            backend.Serialize("value_type", Rendering::MaterialParameterSerializer::ParameterTypeToString(param.Type));
            Rendering::MaterialParameterSerializer::Serialize(backend, "value", param.Value);
            backend.EndObject();
        }

        // Deserialize single parameter (parameter name provided externally)
        static bool Deserialize(SerializationBackend& backend, const std::string& name, Rendering::MaterialParameter& param) {
            backend.BeginObject(name);

            if (!backend.HasField("value") || !backend.HasField("value_type")) {
                backend.EndObject();
                return false;
            }

            param.Name = name;

            std::string typeStr;
            backend.Deserialize("value_type", typeStr);
            param.Type = Rendering::MaterialParameterSerializer::StringToParameterType(typeStr);

            Rendering::MaterialParameterSerializer::Deserialize(backend, "value", param.Value, param.Type);

            backend.EndObject();
            return true;
        }
    };

    // Material serialization
    template<>
    struct Serializer<Rendering::Material> {
        static void Serialize(SerializationBackend& backend, const std::string& name, const Rendering::Material& material) {
            if (!name.empty())
                backend.BeginObject(name);

            // Basic properties
            backend.Serialize("name", material.GetName());
            backend.Serialize("material_type", Rendering::MaterialTypeToString(material.GetType()));

            // Shader path
            std::string shaderPath = "";
            if (material.GetShader()) {
                shaderPath = material.GetShader()->GetPath();
            }
            backend.Serialize("shader_path", shaderPath);

            // Parameters (object format: "paramName": { "value_type": "...", "value": ... })
            const auto& parameters = material.GetParameters();
            backend.BeginObject("parameters");
            for (const auto& [paramName, param] : parameters) {
                Serializer<Rendering::MaterialParameter>::Serialize(backend, paramName, param);
            }
            backend.EndObject();

            // Texture slots
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

            // Check required fields
            if (!(backend.HasField("material_type") &&
                backend.HasField("name") &&
                backend.HasField("parameters"))) {
                if (!name.empty())
                    backend.EndObject();
                return false;
            }

            std::string shaderPath;
            if (backend.HasField("shader_path")) {
                backend.Deserialize("shader_path", shaderPath);
            }

            std::string typeStr;
            backend.Deserialize("material_type", typeStr);
            material.SetType(Rendering::StringToMaterialType(typeStr));

            std::string matName;
            backend.Deserialize("name", matName);
            material.SetName(matName);

            // Load shader from path
            if (!shaderPath.empty()) {
                auto shader = Rendering::Shader::CreateFromFile(shaderPath);
                material.SetShader(shader);
            }

            // Parameters (object format, iterate using ForEachField)
            backend.BeginObject("parameters");
            backend.ForEachField([&](const std::string& paramName) {
                Rendering::MaterialParameter param;
                // ForEachField has switched context to the value node for paramName
                if (backend.HasField("value") && backend.HasField("value_type")) {
                    param.Name = paramName;

                    std::string typeStr;
                    backend.Deserialize("value_type", typeStr);
                    param.Type = Rendering::MaterialParameterSerializer::StringToParameterType(typeStr);

                    Rendering::MaterialParameterSerializer::Deserialize(backend, "value", param.Value, param.Type);

                    material.AddParameter(param);
                }
            });
            backend.EndObject();

            // Texture slots (iterate using ForEachField)
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

    // Material instance serialization
    template<>
    struct Serializer<Rendering::MaterialInstance> {
        static void Serialize(SerializationBackend& backend, const std::string& name, const Rendering::MaterialInstance& instance) {
            if (!name.empty())
                backend.BeginObject(name);

            // Base material name
            const auto baseMaterial = instance.GetBaseMaterial();
            backend.Serialize("base_material_name", baseMaterial ? baseMaterial->GetName() : "");

            // Parameter overrides keep value_type because they are dynamic variant payloads.
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
            if (!baseMaterialName.empty() && Rendering::MaterialLibrary::Instance().HasMaterial(baseMaterialName)) {
                instance.SetBaseMaterial(Rendering::MaterialLibrary::Instance().GetMaterial(baseMaterialName));
            } else if (!baseMaterialName.empty() && !instance.GetBaseMaterial()) {
                instance.SetBaseMaterial(Rendering::Material::Create(baseMaterialName, Rendering::MaterialType::Custom));
            }

            // Clear existing parameter overrides
            instance.ClearParameterOverrides();

            // Parameter overrides (object format, iterate using ForEachField)
            backend.BeginObject("parameter_overrides");
            backend.ForEachField([&](const std::string& paramName) {
                // ForEachField has switched context to the value node for paramName
                if (backend.HasField("value") && backend.HasField("value_type")) {
                    std::string typeStr;
                    backend.Deserialize("value_type", typeStr);
                    auto type = Rendering::MaterialParameterSerializer::StringToParameterType(typeStr);

                    Rendering::MaterialParameterValue value;
                    Rendering::MaterialParameterSerializer::Deserialize(backend, "value", value, type);

                    if (instance.GetBaseMaterial() && instance.GetBaseMaterial()->HasParameter(paramName)) {
                        instance.SetParameter(paramName, value);
                    } else if (instance.GetBaseMaterial()) {
                        instance.GetBaseMaterial()->AddParameter({ paramName, type, value });
                        instance.SetParameter(paramName, value);
                    }
                }
            });
            backend.EndObject();

            if (!name.empty())
                backend.EndObject();

            return true;
        }
    };

    // Convenience functions for Material serialization
    inline bool SaveMaterial(const Rendering::Material& material, const std::string& filename, SerializationFormat format = SerializationFormat::JSON) {
        return SERIALIZE_TO_FILE(material, filename, format);
    }

    inline bool LoadMaterial(const std::string& filename, Rendering::Material& material, SerializationFormat format = SerializationFormat::JSON) {
        return DESERIALIZE_FROM_FILE(filename, material, format);
    }

    // Convenience functions for MaterialInstance serialization
    inline bool SaveMaterialInstance(const Rendering::MaterialInstance& instance, const std::string& filename, SerializationFormat format = SerializationFormat::JSON) {
        return SERIALIZE_TO_FILE(instance, filename, format);
    }

    inline bool LoadMaterialInstance(const std::string& filename, Rendering::MaterialInstance& instance, SerializationFormat format = SerializationFormat::JSON) {
        return DESERIALIZE_FROM_FILE(filename, instance, format);
    }

} // namespace HE::Serialization
