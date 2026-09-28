#include "HuaEngine/Reflection/Reflection.h"
#include "HuaEngine/Serialization/SerializationCore.h"

namespace HE::Refl {
    void SerializeRuntimeObject(const RuntimeTypeDescriptor& type,
        Serialization::SerializationBackend& backend, const std::string& name, const void* object) {
        backend.BeginObject(name);
        for (const RuntimeFieldDescriptor& field : type.Fields) {
            if (HasRuntimeFieldFlag(field.Flags, RuntimeFieldFlags::Serializable) && field.Serialize) {
                field.Serialize(backend, std::string(field.Name), object);
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
            if (backend.HasField(fieldName) && !field.Deserialize(backend, fieldName, object)) { success = false; }
        }
        backend.EndObject();
        return success;
    }
}
