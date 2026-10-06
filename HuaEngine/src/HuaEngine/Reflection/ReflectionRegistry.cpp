#include "HuaEngine/Reflection/ReflectionRegistry.h"

#include <exception>
#include <mutex>

namespace {
    bool EquivalentMetadata(std::span<const std::string_view> leftFlags,
                            std::span<const HE::Refl::RuntimeAttribute> leftAttributes,
                            std::span<const std::string_view> rightFlags,
                            std::span<const HE::Refl::RuntimeAttribute> rightAttributes) noexcept {
        if (leftFlags.size() != rightFlags.size() || leftAttributes.size() != rightAttributes.size()) return false;
        for (size_t index = 0; index < leftFlags.size(); ++index) {
            if (leftFlags[index] != rightFlags[index]) return false;
        }
        for (size_t index = 0; index < leftAttributes.size(); ++index) {
            if (leftAttributes[index].Name != rightAttributes[index].Name ||
                leftAttributes[index].Value != rightAttributes[index].Value) return false;
        }
        return true;
    }

    bool ValidMetadata(std::span<const std::string_view> flags,
                       std::span<const HE::Refl::RuntimeAttribute> attributes) noexcept {
        for (size_t index = 0; index < flags.size(); ++index) {
            if (flags[index].empty()) return false;
            for (size_t prior = 0; prior < index; ++prior) {
                if (flags[prior] == flags[index]) return false;
            }
        }
        for (size_t index = 0; index < attributes.size(); ++index) {
            if (attributes[index].Name.empty()) return false;
            for (size_t prior = 0; prior < index; ++prior) {
                if (attributes[prior].Name == attributes[index].Name) return false;
            }
        }
        return true;
    }

    std::string_view EnumName(const HE::Refl::RuntimeFieldDescriptor& field) noexcept {
        return field.EnumQualifiedName.empty() && field.EnumType
            ? field.EnumType->QualifiedName : field.EnumQualifiedName;
    }

    bool EquivalentType(const HE::Refl::RuntimeTypeDescriptor& left,
                        const HE::Refl::RuntimeTypeDescriptor& right) noexcept {
        if (left.Guid != right.Guid ||
            !EquivalentMetadata(left.Flags, left.Attributes, right.Flags, right.Attributes) ||
            left.Name != right.Name || left.QualifiedName != right.QualifiedName ||
            left.Kind != right.Kind || left.DisplayName != right.DisplayName ||
            left.Category != right.Category || left.Size != right.Size ||
            left.ConstructDefault != right.ConstructDefault || left.Destroy != right.Destroy ||
            left.Copy != right.Copy || left.Serialize != right.Serialize ||
            left.Deserialize != right.Deserialize || left.Fields.size() != right.Fields.size()) return false;
        for (size_t index = 0; index < left.Fields.size(); ++index) {
            const auto& a = left.Fields[index];
            const auto& b = right.Fields[index];
            if (a.Name != b.Name || a.Type != b.Type || a.DisplayName != b.DisplayName ||
                a.Category != b.Category || a.Offset != b.Offset || a.Size != b.Size ||
                a.Flags != b.Flags || a.ValueTypeGuid != b.ValueTypeGuid ||
                !EquivalentMetadata(a.MetadataFlags, a.Attributes, b.MetadataFlags, b.Attributes) ||
                a.GetConst != b.GetConst || a.GetMutable != b.GetMutable ||
                a.Serialize != b.Serialize || a.Deserialize != b.Deserialize || EnumName(a) != EnumName(b)) return false;
        }
        return true;
    }
}

namespace HE::Refl::Detail {
    struct OwnedMetadata {
        struct AttributeText {
            std::string Name;
            std::string Value;
        };

        std::vector<std::string> FlagText;
        std::vector<std::string_view> Flags;
        std::vector<AttributeText> AttributeTexts;
        std::vector<RuntimeAttribute> Attributes;

        OwnedMetadata(std::span<const std::string_view> flags,
                      std::span<const RuntimeAttribute> attributes) {
            FlagText.reserve(flags.size());
            Flags.reserve(flags.size());
            AttributeTexts.reserve(attributes.size());
            Attributes.reserve(attributes.size());
            for (std::string_view flag : flags) FlagText.emplace_back(flag);
            for (const auto& flag : FlagText) Flags.push_back(flag);
            for (const auto& attribute : attributes)
                AttributeTexts.push_back({std::string(attribute.Name), std::string(attribute.Value)});
            for (const auto& attribute : AttributeTexts)
                Attributes.push_back({attribute.Name, attribute.Value});
        }
    };

    struct OwnedEnum {
        struct ValueText {
            std::string Name;
            std::string DisplayName;
        };

        std::string Name;
        std::string QualifiedName;
        std::string UnderlyingType;
        std::vector<ValueText> Text;
        OwnedMetadata Metadata;
        std::vector<OwnedMetadata> ValueMetadata;
        std::vector<RuntimeEnumValueDescriptor> Values;
        RuntimeEnumDescriptor Descriptor;

        explicit OwnedEnum(const RuntimeEnumDescriptor& source)
            : Name(source.Name), QualifiedName(source.QualifiedName), UnderlyingType(source.UnderlyingType),
              Metadata(source.Flags, source.Attributes) {
            Text.reserve(source.Values.size());
            ValueMetadata.reserve(source.Values.size());
            Values.reserve(source.Values.size());
            for (const auto& value : source.Values) {
                Text.push_back({std::string(value.Name), std::string(value.DisplayName)});
                ValueMetadata.emplace_back(value.Flags, value.Attributes);
            }
            for (size_t index = 0; index < source.Values.size(); ++index) {
                Values.push_back({Text[index].Name, source.Values[index].Value, Text[index].DisplayName,
                    ValueMetadata[index].Flags, ValueMetadata[index].Attributes});
            }
            Descriptor = {Name, QualifiedName, UnderlyingType, Values};
            Descriptor.Guid = source.Guid;
            Descriptor.Flags = Metadata.Flags;
            Descriptor.Attributes = Metadata.Attributes;
        }
    };

    struct OwnedType {
        struct FieldText {
            std::string Name;
            std::string Type;
            std::string DisplayName;
            std::string Category;
            std::string EnumQualifiedName;
        };

        std::string Name;
        std::string QualifiedName;
        std::string Kind;
        std::string DisplayName;
        std::string Category;
        std::vector<FieldText> Text;
        OwnedMetadata Metadata;
        std::vector<OwnedMetadata> FieldMetadata;
        std::vector<RuntimeFieldDescriptor> Fields;
        RuntimeTypeDescriptor Descriptor;
        const void* NativeKey = nullptr;

        OwnedType(const RuntimeTypeDescriptor& source, const void* nativeKey)
            : Name(source.Name), QualifiedName(source.QualifiedName), Kind(source.Kind),
              DisplayName(source.DisplayName), Category(source.Category),
              Metadata(source.Flags, source.Attributes), NativeKey(nativeKey) {
            Text.reserve(source.Fields.size());
            FieldMetadata.reserve(source.Fields.size());
            Fields.reserve(source.Fields.size());
            for (const auto& field : source.Fields) {
                Text.push_back({std::string(field.Name), std::string(field.Type),
                    std::string(field.DisplayName), std::string(field.Category), std::string(EnumName(field))});
                FieldMetadata.emplace_back(field.MetadataFlags, field.Attributes);
            }
            for (size_t index = 0; index < source.Fields.size(); ++index) {
                auto field = source.Fields[index];
                field.Name = Text[index].Name;
                field.Type = Text[index].Type;
                field.DisplayName = Text[index].DisplayName;
                field.Category = Text[index].Category;
                field.EnumQualifiedName = Text[index].EnumQualifiedName;
                field.EnumType = nullptr;
                field.MetadataFlags = FieldMetadata[index].Flags;
                field.Attributes = FieldMetadata[index].Attributes;
                Fields.push_back(field);
            }
            Descriptor = source;
            Descriptor.Name = Name;
            Descriptor.QualifiedName = QualifiedName;
            Descriptor.Kind = Kind;
            Descriptor.DisplayName = DisplayName;
            Descriptor.Category = Category;
            Descriptor.Fields = Fields;
            Descriptor.Flags = Metadata.Flags;
            Descriptor.Attributes = Metadata.Attributes;
        }
    };
}

namespace HE::Refl {
    Registry::Registry() = default;
    Registry::~Registry() = default;

    std::optional<RegistrationError> Registry::RegisterEnum(const RuntimeEnumDescriptor& descriptor) {
        if (descriptor.Name.empty() || descriptor.QualifiedName.empty() || descriptor.UnderlyingType.empty()) {
            return RegistrationError{RegistrationErrorCode::InvalidType,
                "Enum name, qualified name and underlying type are required"};
        }
        if (!ValidMetadata(descriptor.Flags, descriptor.Attributes)) {
            return RegistrationError{RegistrationErrorCode::InvalidType, "Enum flags or attributes are invalid"};
        }
        for (size_t index = 0; index < descriptor.Values.size(); ++index) {
            const auto& value = descriptor.Values[index];
            if (value.Name.empty() || !ValidMetadata(value.Flags, value.Attributes)) {
                return RegistrationError{RegistrationErrorCode::InvalidType, "Enum value metadata is invalid"};
            }
            for (size_t prior = 0; prior < index; ++prior) {
                if (descriptor.Values[prior].Name == value.Name) {
                    return RegistrationError{RegistrationErrorCode::InvalidType, "Enum value names must be unique"};
                }
            }
        }
        RuntimeEnumDescriptor resolved = descriptor;
        if (!resolved.Guid) resolved.Guid = TypeGuid::FromName(resolved.QualifiedName);
        std::unique_lock lock(m_Mutex);
        if (const auto found = m_Enums.find(descriptor.QualifiedName); found != m_Enums.end()) {
            const auto& previous = found->second->Descriptor;
            bool equivalent = previous.Guid == resolved.Guid && previous.Name == descriptor.Name &&
                previous.UnderlyingType == descriptor.UnderlyingType &&
                EquivalentMetadata(previous.Flags, previous.Attributes, descriptor.Flags, descriptor.Attributes) &&
                previous.Values.size() == descriptor.Values.size();
            for (size_t index = 0; equivalent && index < descriptor.Values.size(); ++index) {
                const auto& left = previous.Values[index];
                const auto& right = descriptor.Values[index];
                equivalent = left.Name == right.Name && left.Value == right.Value &&
                    left.DisplayName == right.DisplayName &&
                    EquivalentMetadata(left.Flags, left.Attributes, right.Flags, right.Attributes);
            }
            if (equivalent) return std::nullopt;
            return RegistrationError{RegistrationErrorCode::DuplicateType,
                "The enum qualified name is registered with a different definition: " + std::string(descriptor.QualifiedName)};
        }
        if (m_EnumsByGuid.contains(resolved.Guid) || m_TypesByGuid.contains(resolved.Guid)) {
            return RegistrationError{RegistrationErrorCode::DuplicateType,
                "The Guid is already registered with another reflected declaration: " + std::string(descriptor.QualifiedName)};
        }
        bool enumInserted = false;
        bool guidInserted = false;
        try {
            auto owned = std::make_unique<Detail::OwnedEnum>(resolved);
            const auto* published = &owned->Descriptor;
            if (m_AllEnums.size() == m_AllEnums.capacity())
                m_AllEnums.reserve(m_AllEnums.capacity() ? m_AllEnums.capacity() * 2 : 8);
            enumInserted = m_Enums.emplace(published->QualifiedName, std::move(owned)).second;
            guidInserted = m_EnumsByGuid.emplace(published->Guid, published).second;
            m_AllEnums.push_back(published);
            return std::nullopt;
        }
        catch (const std::exception& exception) {
            if (guidInserted) m_EnumsByGuid.erase(resolved.Guid);
            if (enumInserted) m_Enums.erase(descriptor.QualifiedName);
            return RegistrationError{RegistrationErrorCode::ConstructionFailed, exception.what()};
        }
    }

    std::optional<RegistrationError> Registry::RegisterType(
        const RuntimeTypeDescriptor& descriptor, const void* nativeKey) {
        if (descriptor.Name.empty() || descriptor.QualifiedName.empty()) {
            return RegistrationError{RegistrationErrorCode::InvalidType,
                "Type name and qualified name are required"};
        }
        if (!ValidMetadata(descriptor.Flags, descriptor.Attributes)) {
            return RegistrationError{RegistrationErrorCode::InvalidType, "Type flags or attributes are invalid"};
        }
        for (size_t index = 0; index < descriptor.Fields.size(); ++index) {
            const auto& field = descriptor.Fields[index];
            if (field.Name.empty() ||
                !ValidMetadata(field.MetadataFlags, field.Attributes) ||
                (!field.EnumQualifiedName.empty() && field.EnumType &&
                    field.EnumQualifiedName != field.EnumType->QualifiedName)) {
                return RegistrationError{RegistrationErrorCode::InvalidType,
                    "Field name or enum identity is invalid in " + std::string(descriptor.QualifiedName)};
            }
            for (size_t prior = 0; prior < index; ++prior) {
                if (descriptor.Fields[prior].Name == field.Name) {
                    return RegistrationError{RegistrationErrorCode::InvalidType,
                        "Field names must be unique in " + std::string(descriptor.QualifiedName)};
                }
            }
        }
        RuntimeTypeDescriptor resolved = descriptor;
        if (!resolved.Guid) resolved.Guid = TypeGuid::FromName(resolved.QualifiedName);
        std::unique_lock lock(m_Mutex);
        if (const auto found = m_Types.find(descriptor.QualifiedName); found != m_Types.end()) {
            auto& previous = *found->second;
            if (!EquivalentType(previous.Descriptor, resolved) ||
                (previous.NativeKey && nativeKey && previous.NativeKey != nativeKey)) {
                return RegistrationError{RegistrationErrorCode::DuplicateType,
                    "The type qualified name is registered with a different definition: " + std::string(descriptor.QualifiedName)};
            }
            if (nativeKey && !previous.NativeKey) {
                if (m_ByNative.contains(nativeKey)) {
                    return RegistrationError{RegistrationErrorCode::DuplicateType,
                        "The C++ type is registered with another qualified name"};
                }
                try {
                    m_ByNative.emplace(nativeKey, &previous.Descriptor);
                    previous.NativeKey = nativeKey;
                }
                catch (const std::exception& exception) {
                    return RegistrationError{RegistrationErrorCode::ConstructionFailed, exception.what()};
                }
            }
            return std::nullopt;
        }
        if (nativeKey && m_ByNative.contains(nativeKey)) {
            return RegistrationError{RegistrationErrorCode::DuplicateType,
                "The C++ type is registered with another qualified name"};
        }
        if (m_TypesByGuid.contains(resolved.Guid) || m_EnumsByGuid.contains(resolved.Guid)) {
            return RegistrationError{RegistrationErrorCode::DuplicateType,
                "The Guid is already registered with another reflected declaration: " + std::string(descriptor.QualifiedName)};
        }
        for (const auto& field : descriptor.Fields) {
            const auto enumName = EnumName(field);
            if (!enumName.empty() && !m_Enums.contains(enumName)) {
                return RegistrationError{RegistrationErrorCode::InvalidType,
                    "Reflected field requires an unregistered enum: " + std::string(enumName)};
            }
        }
        bool typeInserted = false;
        bool guidInserted = false;
        bool nativeInserted = false;
        try {
            auto owned = std::make_unique<Detail::OwnedType>(resolved, nativeKey);
            for (auto& field : owned->Fields) {
                if (!field.EnumQualifiedName.empty()) {
                    field.EnumType = &m_Enums.find(field.EnumQualifiedName)->second->Descriptor;
                }
            }
            const auto* published = &owned->Descriptor;
            if (m_AllTypes.size() == m_AllTypes.capacity())
                m_AllTypes.reserve(m_AllTypes.capacity() ? m_AllTypes.capacity() * 2 : 8);
            typeInserted = m_Types.emplace(published->QualifiedName, std::move(owned)).second;
            guidInserted = m_TypesByGuid.emplace(published->Guid, published).second;
            if (nativeKey) nativeInserted = m_ByNative.emplace(nativeKey, published).second;
            m_AllTypes.push_back(published);
            return std::nullopt;
        }
        catch (const std::exception& exception) {
            if (nativeInserted) m_ByNative.erase(nativeKey);
            if (guidInserted) m_TypesByGuid.erase(resolved.Guid);
            if (typeInserted) m_Types.erase(descriptor.QualifiedName);
            return RegistrationError{RegistrationErrorCode::ConstructionFailed, exception.what()};
        }
    }

    const RuntimeTypeDescriptor* Registry::FindNative(const void* nativeKey) const {
        std::shared_lock lock(m_Mutex);
        const auto found = m_ByNative.find(nativeKey);
        return found == m_ByNative.end() ? nullptr : found->second;
    }

    const RuntimeTypeDescriptor* Registry::FindByQualifiedName(std::string_view qualifiedName) const {
        std::shared_lock lock(m_Mutex);
        const auto found = m_Types.find(qualifiedName);
        return found == m_Types.end() ? nullptr : &found->second->Descriptor;
    }

    const RuntimeTypeDescriptor* Registry::FindType(TypeGuid guid) const {
        std::shared_lock lock(m_Mutex);
        const auto found = m_TypesByGuid.find(guid);
        return found == m_TypesByGuid.end() ? nullptr : found->second;
    }

    const RuntimeEnumDescriptor* Registry::FindEnum(std::string_view qualifiedName) const {
        std::shared_lock lock(m_Mutex);
        const auto found = m_Enums.find(qualifiedName);
        return found == m_Enums.end() ? nullptr : &found->second->Descriptor;
    }

    const RuntimeEnumDescriptor* Registry::FindEnum(TypeGuid guid) const {
        std::shared_lock lock(m_Mutex);
        const auto found = m_EnumsByGuid.find(guid);
        return found == m_EnumsByGuid.end() ? nullptr : found->second;
    }
}
