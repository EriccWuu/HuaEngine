#pragma once

#include "HuaEngine/ECS/Runtime/TypeRegistry.h"

#include <exception>
#include <new>
#include <utility>

namespace HE::Ecs {
    // The registry owns the descriptor and must outlive every value that borrows it.
    class OwnedValue {
    public:
        OwnedValue() = default;
        ~OwnedValue() { Reset(); }
        OwnedValue(const OwnedValue&) = delete;
        OwnedValue& operator=(const OwnedValue&) = delete;
        OwnedValue(OwnedValue&& other) noexcept;
        OwnedValue& operator=(OwnedValue&& other) noexcept;

        template<typename T, typename... Args>
        static Result<OwnedValue> Construct(const RegisteredType& type, Args&&... args) {
            using Component = std::remove_cvref_t<T>;
            if (type.Descriptor.NativeKey != NativeTypeKey<Component>() ||
                type.Descriptor.Size != sizeof(Component) || type.Descriptor.Alignment != alignof(Component)) {
                return Error{ErrorCode::InvalidType, "Construct", "The descriptor belongs to another C++ type"};
            }
            if constexpr (!std::is_constructible_v<Component, Args...>) {
                return Error{ErrorCode::UnsupportedOperation, "Construct", "The component cannot be constructed from these arguments"};
            }
            else {
                return Prepare(type, "Construct", [&](void* memory) {
                    ::new (memory) Component(std::forward<Args>(args)...);
                });
            }
        }
        [[nodiscard]] static Result<OwnedValue> Default(const RegisteredType& type);
        [[nodiscard]] static Result<OwnedValue> Copy(const RegisteredType& type, const void* source);
        [[nodiscard]] Result<OwnedValue> Clone() const;

        [[nodiscard]] void* Data() noexcept { return m_Data; }
        [[nodiscard]] const void* Data() const noexcept { return m_Data; }
        [[nodiscard]] const RegisteredType* Type() const noexcept { return m_Type; }
        explicit operator bool() const noexcept { return m_Data != nullptr; }
        void Reset() noexcept;
        // A storage slot takes responsibility for Destroy and aligned deallocation.
        [[nodiscard]] void* Release() noexcept;

    private:
        template<typename Constructor>
        static Result<OwnedValue> Prepare(const RegisteredType& type, const char* operation, Constructor&& construct) {
            if (!type.Owner || !type.Owner->Owns(type) || !type.Descriptor.Destroy || type.Descriptor.Storage == StorageKind::Tag) {
                return Error{ErrorCode::InvalidType, operation, "An owned value requires a registered, stored component"};
            }
            void* memory = nullptr;
            try {
                memory = ::operator new(type.Descriptor.Size, std::align_val_t(type.Descriptor.Alignment));
                construct(memory);
                return OwnedValue(type, memory);
            }
            catch (const std::exception& exception) {
                if (memory) ::operator delete(memory, std::align_val_t(type.Descriptor.Alignment));
                return Error{ErrorCode::ConstructionFailed, operation, exception.what()};
            }
            catch (...) {
                if (memory) ::operator delete(memory, std::align_val_t(type.Descriptor.Alignment));
                return Error{ErrorCode::ConstructionFailed, operation, "The component constructor threw a non-standard exception"};
            }
        }
        OwnedValue(const RegisteredType& type, void* memory) noexcept : m_Type(&type), m_Data(memory) {}
        const RegisteredType* m_Type = nullptr;
        void* m_Data = nullptr;
    };
}
