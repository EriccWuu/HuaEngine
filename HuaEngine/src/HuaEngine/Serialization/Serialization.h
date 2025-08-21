#pragma once

#include <string>

namespace HE {
	class ISerialization {
        virtual ~ISerialization() = default;

        virtual void BeginObject() = 0;
        virtual void EndObject() = 0;

        virtual void Serialize(const std::string& name, int value) = 0;
        virtual void Serialize(const std::string& name, float value) = 0;
        virtual void Serialize(const std::string& name, const std::string& value) = 0;

        virtual void Deserialize(const std::string& name, int& value) = 0;
        virtual void Deserialize(const std::string& name, float& value) = 0;
        virtual void Deserialize(const std::string& name, std::string& value) = 0;
	};

    template<typename T>
    struct Serializer {
        static void Serialize(ISerialization& backend, const T& obj) {
            backend.BeginObject();
            auto fieldInfo = Refl::reflect<T>();
            fieldInfo.visit_fields([&backend](auto&&... field) {
                backend.Serialize(field.name, obj.*(field.pointer()));
            });
            backend.EndObject();
        }

        static void Deserialize(ISerialization& backend, T& obj) {
            backend.BeginObject();
            fieldInfo.visit_fields([&backend](auto&&... field) {
                backend.Deserialize(field.name, obj.*(field.pointer()));
            });
            backend.EndObject();
        }
    };
}