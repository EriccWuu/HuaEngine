#pragma once

#include "HuaEngine/ECS/Runtime/Result.h"
#include "HuaEngine/Reflection/ReflectionRegistry.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace HE::Ecs {
    using TypeId = uint32_t;
    inline constexpr TypeId InvalidTypeId = 0;
    using TypeGuid = Refl::TypeGuid;
    using TypeGuidHash = Refl::TypeGuidHash;
    using Refl::ToString;

    enum class StorageKind { Direct, Indirect, Tag };

    template<typename T>
    const void* NativeTypeKey() noexcept {
        return Refl::NativeTypeKey<T>();
    }

    struct TypeDescriptor {
        TypeGuid Guid;
        std::string Name;
        std::string QualifiedName;
        size_t Size = 0;
        size_t Alignment = 0;
        StorageKind Storage = StorageKind::Indirect;
        const void* NativeKey = nullptr;
        const Refl::RuntimeTypeDescriptor* Reflection = nullptr;
        bool TriviallyCopyable = false;
        bool TagCompatible = false;
        bool (*MatchesNativeLayout)(size_t, size_t) noexcept = nullptr;
        void (*DefaultConstruct)(void*) = nullptr;
        void (*Destroy)(void*) noexcept = nullptr;
        void (*CopyConstruct)(void*, const void*) = nullptr;
        // Relocate starts the destination lifetime and ends the source lifetime.
        void (*Relocate)(void*, void*) noexcept = nullptr;

        [[nodiscard]] size_t SlotSize() const noexcept {
            return Storage == StorageKind::Tag ? 0 : (Storage == StorageKind::Indirect ? sizeof(void*) : Size);
        }
        [[nodiscard]] size_t SlotAlignment() const noexcept {
            return Storage == StorageKind::Indirect ? alignof(void*) : Alignment;
        }
        [[nodiscard]] bool CanDefaultConstruct() const noexcept { return DefaultConstruct != nullptr; }
        [[nodiscard]] bool CanCopy() const noexcept { return CopyConstruct != nullptr; }
        [[nodiscard]] bool CanRelocate() const noexcept { return Relocate != nullptr; }
    };

    template<typename T>
    TypeDescriptor MakeTypeDescriptor(TypeGuid guid, std::string name, bool tag = false) {
        using Component = std::remove_cvref_t<T>;
        static_assert(std::is_object_v<Component> && !std::is_array_v<Component>);
        TypeDescriptor descriptor;
        descriptor.Guid = guid;
        descriptor.Name = std::move(name);
        descriptor.QualifiedName = descriptor.Name;
        descriptor.Size = sizeof(Component);
        descriptor.Alignment = alignof(Component);
        descriptor.NativeKey = NativeTypeKey<Component>();
        descriptor.MatchesNativeLayout = [](size_t size, size_t alignment) noexcept {
            return size == sizeof(Component) && alignment == alignof(Component);
        };
        descriptor.TriviallyCopyable = std::is_trivially_copyable_v<Component>;
        descriptor.TagCompatible = std::is_empty_v<Component> && std::is_trivial_v<Component>;
        if constexpr (std::is_default_constructible_v<Component>) {
            descriptor.DefaultConstruct = [](void* destination) { ::new (destination) Component(); };
        }
        if constexpr (std::is_nothrow_destructible_v<Component>) {
            descriptor.Destroy = [](void* object) noexcept { std::destroy_at(static_cast<Component*>(object)); };
        }
        if constexpr (std::is_copy_constructible_v<Component>) {
            descriptor.CopyConstruct = [](void* destination, const void* source) {
                ::new (destination) Component(*static_cast<const Component*>(source));
            };
        }
        if constexpr (std::is_nothrow_destructible_v<Component> && std::is_nothrow_move_constructible_v<Component>) {
            descriptor.Relocate = [](void* destination, void* source) noexcept {
                auto* object = static_cast<Component*>(source);
                ::new (destination) Component(std::move(*object));
                std::destroy_at(object);
            };
        }
        else if constexpr (std::is_nothrow_destructible_v<Component> && std::is_nothrow_copy_constructible_v<Component>) {
            descriptor.Relocate = [](void* destination, void* source) noexcept {
                auto* object = static_cast<Component*>(source);
                ::new (destination) Component(*object);
                std::destroy_at(object);
            };
        }
        descriptor.Storage = tag ? StorageKind::Tag : (descriptor.Relocate ? StorageKind::Direct : StorageKind::Indirect);
        return descriptor;
    }

    // Generated modules may specialize this without requiring a component base class.
    template<typename T>
    struct ComponentTraits {
        static constexpr TypeGuid Guid{};
        static constexpr std::string_view Name{};
        static constexpr bool IsTag = false;
    };

    class TypeRegistry;
    struct RegisteredType {
        RegisteredType();
        ~RegisteredType();
        RegisteredType(const RegisteredType&) = delete;
        RegisteredType& operator=(const RegisteredType&) = delete;
        TypeId Id = InvalidTypeId;
        TypeDescriptor Descriptor;
        const TypeRegistry* Owner = nullptr;
    };

    class TypeRegistry {
    public:
        TypeRegistry();
        ~TypeRegistry();
        TypeRegistry(const TypeRegistry&) = delete;
        TypeRegistry& operator=(const TypeRegistry&) = delete;
        TypeRegistry(TypeRegistry&&) = delete;
        TypeRegistry& operator=(TypeRegistry&&) = delete;

        [[nodiscard]] Result<TypeId> Register(TypeDescriptor descriptor);
        [[nodiscard]] Result<void> RegisterEnum(const Refl::RuntimeEnumDescriptor& descriptor);
        template<typename T>
        [[nodiscard]] Result<TypeId> Register(TypeGuid guid, std::string name, bool tag = false) {
            if (!IsOwnerThread())
                return Error{ErrorCode::WrongThread, "Register", "Types must be registered on the Context owner thread"};
            using Traits = ComponentTraits<std::remove_cvref_t<T>>;
            if constexpr (requires { Traits::Describe(); }) {
                auto descriptor = Traits::Describe();
                if (descriptor.Guid != guid || descriptor.Name != name ||
                    (descriptor.Storage == StorageKind::Tag) != tag) {
                    return Error{ErrorCode::InvalidType, "Register", "Explicit identity differs from the generated component descriptor"};
                }
                if constexpr (requires { Traits::RegisterDependencies(*this); }) {
                    auto dependencies = Traits::RegisterDependencies(*this);
                    if (!dependencies) return dependencies.GetError();
                }
                return Register(std::move(descriptor));
            }
            else {
                return Register(MakeTypeDescriptor<T>(guid, std::move(name), tag));
            }
        }
        template<typename T>
        [[nodiscard]] Result<TypeId> Register(std::string stableName) {
            if (!IsOwnerThread())
                return Error{ErrorCode::WrongThread, "Register", "Types must be registered on the Context owner thread"};
            using Traits = ComponentTraits<std::remove_cvref_t<T>>;
            if constexpr (requires { Traits::Describe(); }) {
                auto descriptor = Traits::Describe();
                if (descriptor.Name != stableName) {
                    return Error{ErrorCode::InvalidType, "Register", "Explicit name differs from the generated component descriptor"};
                }
                if constexpr (requires { Traits::RegisterDependencies(*this); }) {
                    auto dependencies = Traits::RegisterDependencies(*this);
                    if (!dependencies) return dependencies.GetError();
                }
                return Register(std::move(descriptor));
            }
            else {
                const TypeGuid guid = TypeGuid::FromName(stableName);
                return Register<T>(guid, std::move(stableName));
            }
        }
        template<typename T>
        [[nodiscard]] Result<TypeId> Register() {
            if (!IsOwnerThread())
                return Error{ErrorCode::WrongThread, "Register", "Types must be registered on the Context owner thread"};
            using Traits = ComponentTraits<std::remove_cvref_t<T>>;
            if constexpr (requires { Traits::Describe(); }) {
                if constexpr (requires { Traits::RegisterDependencies(*this); }) {
                    auto dependencies = Traits::RegisterDependencies(*this);
                    if (!dependencies) return dependencies.GetError();
                }
                return Register(Traits::Describe());
            }
            else {
                return Register<T>(Traits::Guid, std::string(Traits::Name), Traits::IsTag);
            }
        }
        [[nodiscard]] const RegisteredType* Find(TypeId id) const;
        [[nodiscard]] const RegisteredType* Find(TypeGuid guid) const;
        [[nodiscard]] const RegisteredType* FindByName(std::string_view name) const;
        [[nodiscard]] const RegisteredType* FindByQualifiedName(std::string_view name) const;
        [[nodiscard]] const RegisteredType* FindNative(const void* key) const;
        [[nodiscard]] const Refl::RuntimeEnumDescriptor* FindEnum(std::string_view qualifiedName) const;
        [[nodiscard]] Refl::Registry& Reflection() noexcept { return m_Reflection; }
        [[nodiscard]] const Refl::Registry& Reflection() const noexcept { return m_Reflection; }
        template<typename T>
        [[nodiscard]] const RegisteredType* Find() const { return FindNative(NativeTypeKey<T>()); }
        [[nodiscard]] bool Owns(const RegisteredType& type) const;
        // Enumeration is for the owner thread, outside concurrent registration.
        [[nodiscard]] std::span<const RegisteredType* const> All() const noexcept { return m_All; }
        [[nodiscard]] std::span<const Refl::RuntimeEnumDescriptor* const> AllEnums() const noexcept { return m_Reflection.AllEnums(); }
        [[nodiscard]] bool IsOwnerThread() const noexcept { return m_OwnerThread == std::this_thread::get_id(); }

    private:
        Refl::Registry m_Reflection;
        const std::thread::id m_OwnerThread = std::this_thread::get_id();
        mutable std::shared_mutex m_Mutex;
        std::vector<std::unique_ptr<RegisteredType>> m_Types;
        std::vector<const RegisteredType*> m_All;
        std::unordered_map<TypeGuid, TypeId, TypeGuidHash> m_ByGuid;
        std::unordered_map<std::string_view, TypeId> m_ByName;
        std::unordered_map<std::string_view, TypeId> m_ByQualifiedName;
        std::unordered_map<const void*, TypeId> m_ByNative;
    };
}
