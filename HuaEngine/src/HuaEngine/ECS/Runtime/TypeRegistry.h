#pragma once

#include "HuaEngine/ECS/Runtime/Result.h"

#include <compare>
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

namespace HE::Refl { struct RuntimeTypeDescriptor; }

namespace HE::Ecs {
    using TypeId = uint32_t;
    inline constexpr TypeId InvalidTypeId = 0;

    struct TypeGuid {
        uint64_t High = 0;
        uint64_t Low = 0;
        constexpr explicit operator bool() const noexcept { return High != 0 || Low != 0; }
        constexpr auto operator<=>(const TypeGuid&) const = default;
        static TypeGuid FromString(std::string_view text) noexcept;
        // Explicit names are useful for manually registered, non-generated types.
        static constexpr TypeGuid FromName(std::string_view name) noexcept {
            uint64_t high = 14695981039346656037ULL;
            uint64_t low = 7809847782465536322ULL;
            for (const unsigned char value : name) {
                high = (high ^ value) * 1099511628211ULL;
                low = (low ^ value) * 14029467366897019727ULL;
            }
            return name.empty() ? TypeGuid{} : TypeGuid{high, low};
        }
    };
    [[nodiscard]] std::string ToString(TypeGuid guid);

    struct TypeGuidHash {
        size_t operator()(TypeGuid guid) const noexcept {
            const uint64_t mixed = guid.High ^ (guid.Low + 0x9e3779b97f4a7c15ULL + (guid.High << 6) + (guid.High >> 2));
            if constexpr (sizeof(size_t) < sizeof(uint64_t)) return static_cast<size_t>(mixed ^ (mixed >> 32));
            return static_cast<size_t>(mixed);
        }
    };

    enum class StorageKind { Direct, Indirect, Tag };

    namespace Detail {
        template<typename T> inline unsigned char NativeTypeToken = 0;
    }
    template<typename T>
    const void* NativeTypeKey() noexcept {
        return &Detail::NativeTypeToken<std::remove_cvref_t<T>>;
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
        TypeId Id = InvalidTypeId;
        TypeDescriptor Descriptor;
        const TypeRegistry* Owner = nullptr;
    };

    class TypeRegistry {
    public:
        TypeRegistry() = default;
        TypeRegistry(const TypeRegistry&) = delete;
        TypeRegistry& operator=(const TypeRegistry&) = delete;
        TypeRegistry(TypeRegistry&&) = delete;
        TypeRegistry& operator=(TypeRegistry&&) = delete;

        [[nodiscard]] Result<TypeId> Register(TypeDescriptor descriptor);
        template<typename T>
        [[nodiscard]] Result<TypeId> Register(TypeGuid guid, std::string name, bool tag = false) {
            using Traits = ComponentTraits<std::remove_cvref_t<T>>;
            if constexpr (requires { Traits::Describe(); }) {
                auto descriptor = Traits::Describe();
                if (descriptor.Guid != guid || descriptor.Name != name ||
                    (descriptor.Storage == StorageKind::Tag) != tag) {
                    return Error{ErrorCode::InvalidType, "Register", "Explicit identity differs from the generated component descriptor"};
                }
                return Register(std::move(descriptor));
            }
            else {
                return Register(MakeTypeDescriptor<T>(guid, std::move(name), tag));
            }
        }
        template<typename T>
        [[nodiscard]] Result<TypeId> Register(std::string stableName) {
            using Traits = ComponentTraits<std::remove_cvref_t<T>>;
            if constexpr (requires { Traits::Describe(); }) {
                auto descriptor = Traits::Describe();
                if (descriptor.Name != stableName) {
                    return Error{ErrorCode::InvalidType, "Register", "Explicit name differs from the generated component descriptor"};
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
            using Traits = ComponentTraits<std::remove_cvref_t<T>>;
            if constexpr (requires { Traits::Describe(); }) {
                return Register(Traits::Describe());
            }
            else {
                return Register<T>(Traits::Guid, std::string(Traits::Name), Traits::IsTag);
            }
        }
        [[nodiscard]] const RegisteredType* Find(TypeId id) const;
        [[nodiscard]] const RegisteredType* Find(TypeGuid guid) const;
        [[nodiscard]] const RegisteredType* FindByName(std::string_view name) const;
        [[nodiscard]] const RegisteredType* FindNative(const void* key) const;
        template<typename T>
        [[nodiscard]] const RegisteredType* Find() const { return FindNative(NativeTypeKey<T>()); }
        [[nodiscard]] bool Owns(const RegisteredType& type) const;
        // Enumeration is for the owner thread, outside concurrent registration.
        [[nodiscard]] std::span<const RegisteredType* const> All() const noexcept { return m_All; }
        [[nodiscard]] bool IsOwnerThread() const noexcept { return m_OwnerThread == std::this_thread::get_id(); }

    private:
        const std::thread::id m_OwnerThread = std::this_thread::get_id();
        mutable std::shared_mutex m_Mutex;
        std::vector<std::unique_ptr<RegisteredType>> m_Types;
        std::vector<const RegisteredType*> m_All;
        std::unordered_map<TypeGuid, TypeId, TypeGuidHash> m_ByGuid;
        std::unordered_map<std::string_view, TypeId> m_ByName;
        std::unordered_map<const void*, TypeId> m_ByNative;
    };
}
