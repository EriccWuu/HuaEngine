#pragma once

#include "HuaEngine/Reflection/Reflection.h"

#include <memory>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace HE::Refl {
    namespace Detail {
        template<typename T> inline unsigned char NativeTypeToken = 0;
        struct OwnedType;
        struct OwnedEnum;
    }

    template<typename T>
    const void* NativeTypeKey() noexcept {
        return &Detail::NativeTypeToken<std::remove_cvref_t<T>>;
    }

    enum class RegistrationErrorCode {
        InvalidType,
        DuplicateType,
        ConstructionFailed,
    };

    struct RegistrationError {
        RegistrationErrorCode Code;
        std::string Message;
    };

    class Registry final {
    public:
        Registry();
        ~Registry();
        Registry(const Registry&) = delete;
        Registry& operator=(const Registry&) = delete;
        Registry(Registry&&) = delete;
        Registry& operator=(Registry&&) = delete;

        [[nodiscard]] std::optional<RegistrationError> RegisterEnum(const RuntimeEnumDescriptor& descriptor);
        [[nodiscard]] std::optional<RegistrationError> RegisterType(
            const RuntimeTypeDescriptor& descriptor, const void* nativeKey = nullptr);
        template<typename T>
        [[nodiscard]] std::optional<RegistrationError> RegisterType(const RuntimeTypeDescriptor& descriptor) {
            return RegisterType(descriptor, NativeTypeKey<T>());
        }

        [[nodiscard]] const RuntimeTypeDescriptor* FindNative(const void* nativeKey) const;
        template<typename T>
        [[nodiscard]] const RuntimeTypeDescriptor* Find() const { return FindNative(NativeTypeKey<T>()); }
        [[nodiscard]] const RuntimeTypeDescriptor* FindByQualifiedName(std::string_view qualifiedName) const;
        [[nodiscard]] const RuntimeTypeDescriptor* FindType(TypeGuid guid) const;
        [[nodiscard]] const RuntimeEnumDescriptor* FindEnum(std::string_view qualifiedName) const;
        [[nodiscard]] const RuntimeEnumDescriptor* FindEnum(TypeGuid guid) const;

        // Enumerate only while registration is excluded; the spans can change afterward.
        [[nodiscard]] std::span<const RuntimeTypeDescriptor* const> AllTypes() const noexcept { return m_AllTypes; }
        [[nodiscard]] std::span<const RuntimeEnumDescriptor* const> AllEnums() const noexcept { return m_AllEnums; }

    private:
        mutable std::shared_mutex m_Mutex;
        std::unordered_map<std::string_view, std::unique_ptr<Detail::OwnedType>> m_Types;
        std::unordered_map<TypeGuid, const RuntimeTypeDescriptor*, TypeGuidHash> m_TypesByGuid;
        std::unordered_map<const void*, const RuntimeTypeDescriptor*> m_ByNative;
        std::vector<const RuntimeTypeDescriptor*> m_AllTypes;
        std::unordered_map<std::string_view, std::unique_ptr<Detail::OwnedEnum>> m_Enums;
        std::unordered_map<TypeGuid, const RuntimeEnumDescriptor*, TypeGuidHash> m_EnumsByGuid;
        std::vector<const RuntimeEnumDescriptor*> m_AllEnums;
    };
}
