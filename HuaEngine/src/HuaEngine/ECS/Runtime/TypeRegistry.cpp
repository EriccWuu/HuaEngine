#include "HuaEngine/ECS/Runtime/TypeRegistry.h"
#include "HuaEngine/Reflection/Reflection.h"

#include <limits>
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

    std::string_view EnumName(const HE::Refl::RuntimeFieldDescriptor& field) noexcept {
        return field.EnumQualifiedName.empty() && field.EnumType
            ? field.EnumType->QualifiedName : field.EnumQualifiedName;
    }

    bool EquivalentReflection(const HE::Refl::RuntimeTypeDescriptor* left,
                              const HE::Refl::RuntimeTypeDescriptor* right) noexcept {
        if (!left || !right) return left == right;
        const auto leftGuid = left->Guid ? left->Guid : HE::Refl::TypeGuid::FromName(left->QualifiedName);
        const auto rightGuid = right->Guid ? right->Guid : HE::Refl::TypeGuid::FromName(right->QualifiedName);
        if (leftGuid != rightGuid ||
            !EquivalentMetadata(left->Flags, left->Attributes, right->Flags, right->Attributes) ||
            left->Name != right->Name || left->QualifiedName != right->QualifiedName ||
            left->Kind != right->Kind || left->DisplayName != right->DisplayName ||
            left->Category != right->Category || left->Size != right->Size ||
            left->ConstructDefault != right->ConstructDefault || left->Destroy != right->Destroy ||
            left->Copy != right->Copy || left->Serialize != right->Serialize ||
            left->Deserialize != right->Deserialize ||
            left->Fields.size() != right->Fields.size()) return false;
        for (size_t index = 0; index < left->Fields.size(); ++index) {
            const auto& a = left->Fields[index];
            const auto& b = right->Fields[index];
            if (a.Name != b.Name || a.Type != b.Type || a.DisplayName != b.DisplayName ||
                a.Category != b.Category || a.Offset != b.Offset || a.Size != b.Size ||
                a.Flags != b.Flags ||
                !EquivalentMetadata(a.MetadataFlags, a.Attributes, b.MetadataFlags, b.Attributes) ||
                a.GetConst != b.GetConst || a.GetMutable != b.GetMutable ||
                a.Serialize != b.Serialize || a.Deserialize != b.Deserialize || EnumName(a) != EnumName(b)) return false;
        }
        return true;
    }

    HE::Ecs::Error ReflectionRegistrationError(std::string operation,
                                                HE::Refl::RegistrationError error) {
        HE::Ecs::ErrorCode code = HE::Ecs::ErrorCode::ConstructionFailed;
        if (error.Code == HE::Refl::RegistrationErrorCode::InvalidType)
            code = HE::Ecs::ErrorCode::InvalidType;
        else if (error.Code == HE::Refl::RegistrationErrorCode::DuplicateType)
            code = HE::Ecs::ErrorCode::DuplicateType;
        return HE::Ecs::Error{code, std::move(operation), std::move(error.Message)};
    }
}

namespace HE::Ecs {
    RegisteredType::RegisteredType() = default;
    RegisteredType::~RegisteredType() = default;
    TypeRegistry::TypeRegistry() = default;
    TypeRegistry::~TypeRegistry() = default;

    Result<void> TypeRegistry::RegisterEnum(const Refl::RuntimeEnumDescriptor& descriptor) {
        if (!IsOwnerThread())
            return Error{ErrorCode::WrongThread, "RegisterEnum", "Enums must be registered on the Context owner thread"};
        if (auto error = m_Reflection.RegisterEnum(descriptor))
            return ReflectionRegistrationError("RegisterEnum", std::move(*error));
        return {};
    }

    Result<TypeId> TypeRegistry::Register(TypeDescriptor descriptor) {
        if (!IsOwnerThread()) return Error{ErrorCode::WrongThread, "Register", "Types must be registered on the Context owner thread"};
        if (!descriptor.Reflection && descriptor.NativeKey)
            descriptor.Reflection = m_Reflection.FindNative(descriptor.NativeKey);
        if (descriptor.Reflection) {
            if (descriptor.QualifiedName == descriptor.Name)
                descriptor.QualifiedName = descriptor.Reflection->QualifiedName;
            const auto reflectionGuid = descriptor.Reflection->Guid ? descriptor.Reflection->Guid :
                Refl::TypeGuid::FromName(descriptor.Reflection->QualifiedName);
            if (descriptor.Guid && descriptor.Guid != reflectionGuid) {
                return Error{ErrorCode::InvalidType, "Register", "Component Guid differs from its reflected type Guid"};
            }
            descriptor.Guid = reflectionGuid;
        }
        if (!descriptor.Guid || descriptor.Name.empty() || !descriptor.NativeKey || !descriptor.Destroy ||
            !descriptor.MatchesNativeLayout || !descriptor.MatchesNativeLayout(descriptor.Size, descriptor.Alignment) ||
            descriptor.QualifiedName.empty() || descriptor.Size == 0 || descriptor.Alignment == 0 ||
            (descriptor.Alignment & (descriptor.Alignment - 1)) != 0 ||
            descriptor.Size % descriptor.Alignment != 0 ||
            (descriptor.Storage != StorageKind::Direct && descriptor.Storage != StorageKind::Indirect && descriptor.Storage != StorageKind::Tag) ||
            (descriptor.Storage == StorageKind::Direct && !descriptor.Relocate) ||
            (descriptor.Storage == StorageKind::Tag && !descriptor.TagCompatible) ||
            (descriptor.Reflection && (descriptor.Reflection->QualifiedName != descriptor.QualifiedName ||
                descriptor.Reflection->Size != descriptor.Size))) {
            return Error{ErrorCode::InvalidType, "Register", "A stable identity, native type and valid no-throw lifetime operations are required"};
        }
        std::unique_lock lock(m_Mutex);
        if (const auto existing = m_ByGuid.find(descriptor.Guid); existing != m_ByGuid.end()) {
            const auto& registered = *m_Types[existing->second - 1];
            const auto& previous = registered.Descriptor;
            if (previous.NativeKey == descriptor.NativeKey && previous.Name == descriptor.Name &&
                previous.QualifiedName == descriptor.QualifiedName && previous.Size == descriptor.Size &&
                previous.Alignment == descriptor.Alignment && previous.Storage == descriptor.Storage &&
                previous.MatchesNativeLayout == descriptor.MatchesNativeLayout &&
                EquivalentReflection(previous.Reflection, descriptor.Reflection) && previous.Destroy == descriptor.Destroy &&
                previous.DefaultConstruct == descriptor.DefaultConstruct && previous.CopyConstruct == descriptor.CopyConstruct &&
                previous.Relocate == descriptor.Relocate) return existing->second;
            return Error{ErrorCode::DuplicateType, "Register", "The Guid is already registered with a different descriptor: " + descriptor.Name};
        }
        if (m_ByName.contains(descriptor.Name) || m_ByQualifiedName.contains(descriptor.QualifiedName) ||
            m_ByNative.contains(descriptor.NativeKey)) {
            return Error{ErrorCode::DuplicateType, "Register", "The name, qualified name or C++ type already has another Guid: " + descriptor.Name};
        }
        if (m_Types.size() >= std::numeric_limits<TypeId>::max()) {
            return Error{ErrorCode::InvalidState, "Register", "The TypeId space is exhausted"};
        }
        std::unique_ptr<RegisteredType> entry;
        bool guidInserted = false;
        bool nameInserted = false;
        bool qualifiedNameInserted = false;
        bool nativeInserted = false;
        try {
            const auto* source = descriptor.Reflection;
            entry = std::make_unique<RegisteredType>();
            entry->Id = static_cast<TypeId>(m_Types.size() + 1);
            entry->Descriptor = std::move(descriptor);
            entry->Owner = this;
            // Reserve before publishing any entry; all remaining vector operations are non-throwing.
            if (m_Types.size() == m_Types.capacity()) m_Types.reserve(m_Types.capacity() ? m_Types.capacity() * 2 : 8);
            if (m_All.size() == m_All.capacity()) m_All.reserve(m_All.capacity() ? m_All.capacity() * 2 : 8);
            guidInserted = m_ByGuid.emplace(entry->Descriptor.Guid, entry->Id).second;
            nameInserted = m_ByName.emplace(entry->Descriptor.Name, entry->Id).second;
            qualifiedNameInserted = m_ByQualifiedName.emplace(entry->Descriptor.QualifiedName, entry->Id).second;
            nativeInserted = m_ByNative.emplace(entry->Descriptor.NativeKey, entry->Id).second;
            if (source) {
                if (auto error = m_Reflection.RegisterType(*source, entry->Descriptor.NativeKey)) {
                    if (nativeInserted) m_ByNative.erase(entry->Descriptor.NativeKey);
                    if (qualifiedNameInserted) m_ByQualifiedName.erase(entry->Descriptor.QualifiedName);
                    if (nameInserted) m_ByName.erase(entry->Descriptor.Name);
                    if (guidInserted) m_ByGuid.erase(entry->Descriptor.Guid);
                    return ReflectionRegistrationError("Register", std::move(*error));
                }
                entry->Descriptor.Reflection = m_Reflection.FindNative(entry->Descriptor.NativeKey);
            }
            const TypeId id = entry->Id;
            m_All.push_back(entry.get());
            m_Types.push_back(std::move(entry));
            return id;
        }
        catch (const std::exception& exception) {
            if (entry) {
                if (nativeInserted) m_ByNative.erase(entry->Descriptor.NativeKey);
                if (qualifiedNameInserted) m_ByQualifiedName.erase(entry->Descriptor.QualifiedName);
                if (nameInserted) m_ByName.erase(entry->Descriptor.Name);
                if (guidInserted) m_ByGuid.erase(entry->Descriptor.Guid);
            }
            return Error{ErrorCode::ConstructionFailed, "Register", exception.what()};
        }
    }

    const RegisteredType* TypeRegistry::Find(TypeId id) const {
        std::shared_lock lock(m_Mutex);
        return id != InvalidTypeId && id <= m_Types.size() ? m_Types[id - 1].get() : nullptr;
    }
    const RegisteredType* TypeRegistry::Find(TypeGuid guid) const {
        std::shared_lock lock(m_Mutex);
        const auto found = m_ByGuid.find(guid);
        return found == m_ByGuid.end() ? nullptr : m_Types[found->second - 1].get();
    }
    const RegisteredType* TypeRegistry::FindByName(std::string_view name) const {
        std::shared_lock lock(m_Mutex);
        const auto found = m_ByName.find(name);
        return found == m_ByName.end() ? nullptr : m_Types[found->second - 1].get();
    }
    const RegisteredType* TypeRegistry::FindByQualifiedName(std::string_view name) const {
        std::shared_lock lock(m_Mutex);
        const auto found = m_ByQualifiedName.find(name);
        return found == m_ByQualifiedName.end() ? nullptr : m_Types[found->second - 1].get();
    }
    const RegisteredType* TypeRegistry::FindNative(const void* key) const {
        std::shared_lock lock(m_Mutex);
        const auto found = m_ByNative.find(key);
        return found == m_ByNative.end() ? nullptr : m_Types[found->second - 1].get();
    }
    const Refl::RuntimeEnumDescriptor* TypeRegistry::FindEnum(std::string_view qualifiedName) const {
        return m_Reflection.FindEnum(qualifiedName);
    }
    bool TypeRegistry::Owns(const RegisteredType& type) const {
        std::shared_lock lock(m_Mutex);
        return type.Owner == this && type.Id != InvalidTypeId && type.Id <= m_Types.size() &&
            m_Types[type.Id - 1].get() == &type;
    }
}
