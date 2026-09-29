#include "HuaEngine/Reflection/Reflection.h"
#include "HuaEngine/Serialization/SerializationCore.h"

namespace HE::Refl {
    const RuntimeEnumValueDescriptor* FindRuntimeEnumValueByName(
        const RuntimeEnumDescriptor& enumType, std::string_view name) {
        for (const auto& value : enumType.Values) {
            if (value.Name == name) return &value;
        }
        return nullptr;
    }

    const RuntimeEnumValueDescriptor* FindRuntimeEnumValueByValue(
        const RuntimeEnumDescriptor& enumType, int64_t sought) {
        for (const auto& value : enumType.Values) {
            if (value.Value == sought) return &value;
        }
        return nullptr;
    }

    void SerializeRuntimeObject(const RuntimeTypeDescriptor& type,
        Serialization::SerializationBackend& backend, const std::string& name, const void* object) {
        backend.BeginObject(name);
        for (const RuntimeFieldDescriptor& field : type.Fields) {
            if (HasRuntimeFieldFlag(field.Flags, RuntimeFieldFlags::Serializable) && field.Serialize) {
                field.Serialize(field, backend, std::string(field.Name), object);
            }
        }
        backend.EndObject();
    }

    bool DeserializeRuntimeObject(const RuntimeTypeDescriptor& type,
        Serialization::SerializationBackend& backend, const std::string& name, void* object) {
        if (!name.empty() && (!backend.HasField(name) ||
            backend.GetFieldType(name) != Serialization::SerializationType::Object)) { return false; }
        backend.BeginObject(name);
        bool success = true;
        for (const RuntimeFieldDescriptor& field : type.Fields) {
            if (!HasRuntimeFieldFlag(field.Flags, RuntimeFieldFlags::Serializable) || !field.Deserialize) { continue; }
            const std::string fieldName(field.Name);
            if (backend.HasField(fieldName) && !field.Deserialize(field, backend, fieldName, object)) { success = false; }
        }
        backend.EndObject();
        return success;
    }
}
