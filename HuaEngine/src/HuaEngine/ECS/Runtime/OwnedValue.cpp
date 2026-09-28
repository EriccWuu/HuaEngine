#include "HuaEngine/ECS/Runtime/OwnedValue.h"

namespace HE::Ecs {
    OwnedValue::OwnedValue(OwnedValue&& other) noexcept
        : m_Type(std::exchange(other.m_Type, nullptr)), m_Data(std::exchange(other.m_Data, nullptr)) {}

    OwnedValue& OwnedValue::operator=(OwnedValue&& other) noexcept {
        if (this != &other) {
            Reset();
            m_Type = std::exchange(other.m_Type, nullptr);
            m_Data = std::exchange(other.m_Data, nullptr);
        }
        return *this;
    }
    Result<OwnedValue> OwnedValue::Default(const RegisteredType& type) {
        if (!type.Descriptor.DefaultConstruct) {
            return Error{ErrorCode::UnsupportedOperation, "DefaultConstruct", "The component has no default constructor: " + type.Descriptor.Name};
        }
        return Prepare(type, "DefaultConstruct", type.Descriptor.DefaultConstruct);
    }
    Result<OwnedValue> OwnedValue::Copy(const RegisteredType& type, const void* source) {
        if (!source) return Error{ErrorCode::InvalidArgument, "CopyConstruct", "The source object is null"};
        if (!type.Descriptor.CopyConstruct) {
            return Error{ErrorCode::UnsupportedOperation, "CopyConstruct", "The component is not copy constructible: " + type.Descriptor.Name};
        }
        return Prepare(type, "CopyConstruct", [&](void* memory) { type.Descriptor.CopyConstruct(memory, source); });
    }
    Result<OwnedValue> OwnedValue::Clone() const {
        if (!m_Type || !m_Data) return Error{ErrorCode::InvalidState, "Clone", "The value is empty"};
        return Copy(*m_Type, m_Data);
    }
    void OwnedValue::Reset() noexcept {
        if (m_Data) {
            m_Type->Descriptor.Destroy(m_Data);
            ::operator delete(m_Data, std::align_val_t(m_Type->Descriptor.Alignment));
        }
        m_Data = nullptr;
        m_Type = nullptr;
    }
    void* OwnedValue::Release() noexcept {
        m_Type = nullptr;
        return std::exchange(m_Data, nullptr);
    }
}
