#include "HuaEngine/ECS/Runtime/TypeRegistry.h"

#include <limits>
#include <mutex>

namespace HE::Ecs {
    TypeGuid TypeGuid::FromString(std::string_view text) noexcept {
        if (text.size() != 32 && text.size() != 36) return {};
        TypeGuid result;
        size_t digit = 0;
        for (size_t index = 0; index < text.size(); ++index) {
            if (text.size() == 36 && (index == 8 || index == 13 || index == 18 || index == 23)) {
                if (text[index] != '-') return {};
                continue;
            }
            const char value = text[index];
            const int hex = value >= '0' && value <= '9' ? value - '0' :
                (value >= 'a' && value <= 'f' ? value - 'a' + 10 :
                 (value >= 'A' && value <= 'F' ? value - 'A' + 10 : -1));
            if (hex < 0) return {};
            auto& half = digit++ < 16 ? result.High : result.Low;
            half = (half << 4) | static_cast<uint64_t>(hex);
        }
        return digit == 32 ? result : TypeGuid{};
    }

    std::string ToString(TypeGuid guid) {
        constexpr char digits[] = "0123456789abcdef";
        std::string text(32, '0');
        for (int index = 31; index >= 0; --index) {
            auto& half = index >= 16 ? guid.Low : guid.High;
            text[static_cast<size_t>(index)] = digits[half & 15];
            half >>= 4;
        }
        return text;
    }

    Result<TypeId> TypeRegistry::Register(TypeDescriptor descriptor) {
        if (!IsOwnerThread()) return Error{ErrorCode::WrongThread, "Register", "Types must be registered on the Context owner thread"};
        if (!descriptor.Guid || descriptor.Name.empty() || !descriptor.NativeKey || !descriptor.Destroy ||
            !descriptor.MatchesNativeLayout || !descriptor.MatchesNativeLayout(descriptor.Size, descriptor.Alignment) ||
            descriptor.Size == 0 || descriptor.Alignment == 0 ||
            (descriptor.Alignment & (descriptor.Alignment - 1)) != 0 ||
            descriptor.Size % descriptor.Alignment != 0 ||
            (descriptor.Storage != StorageKind::Direct && descriptor.Storage != StorageKind::Indirect && descriptor.Storage != StorageKind::Tag) ||
            (descriptor.Storage == StorageKind::Direct && !descriptor.Relocate) ||
            (descriptor.Storage == StorageKind::Tag && !descriptor.TagCompatible)) {
            return Error{ErrorCode::InvalidType, "Register", "A stable identity, native type and valid no-throw lifetime operations are required"};
        }
        std::unique_lock lock(m_Mutex);
        if (const auto existing = m_ByGuid.find(descriptor.Guid); existing != m_ByGuid.end()) {
            const auto& previous = m_Types[existing->second - 1]->Descriptor;
            if (previous.NativeKey == descriptor.NativeKey && previous.Name == descriptor.Name &&
                previous.QualifiedName == descriptor.QualifiedName && previous.Size == descriptor.Size &&
                previous.Alignment == descriptor.Alignment && previous.Storage == descriptor.Storage &&
                previous.MatchesNativeLayout == descriptor.MatchesNativeLayout &&
                previous.Reflection == descriptor.Reflection && previous.Destroy == descriptor.Destroy &&
                previous.DefaultConstruct == descriptor.DefaultConstruct && previous.CopyConstruct == descriptor.CopyConstruct &&
                previous.Relocate == descriptor.Relocate) return existing->second;
            return Error{ErrorCode::DuplicateType, "Register", "The Guid is already registered with a different descriptor: " + descriptor.Name};
        }
        if (m_ByName.contains(descriptor.Name) || m_ByNative.contains(descriptor.NativeKey)) {
            return Error{ErrorCode::DuplicateType, "Register", "The name or C++ type already has another Guid: " + descriptor.Name};
        }
        if (m_Types.size() >= std::numeric_limits<TypeId>::max()) {
            return Error{ErrorCode::InvalidState, "Register", "The TypeId space is exhausted"};
        }
        std::unique_ptr<RegisteredType> entry;
        bool guidInserted = false;
        bool nameInserted = false;
        bool nativeInserted = false;
        try {
            entry = std::make_unique<RegisteredType>(RegisteredType{
                static_cast<TypeId>(m_Types.size() + 1), std::move(descriptor), this});
            // Reserve before publishing any entry; all remaining vector operations are non-throwing.
            if (m_Types.size() == m_Types.capacity()) m_Types.reserve(m_Types.capacity() ? m_Types.capacity() * 2 : 8);
            if (m_All.size() == m_All.capacity()) m_All.reserve(m_All.capacity() ? m_All.capacity() * 2 : 8);
            guidInserted = m_ByGuid.emplace(entry->Descriptor.Guid, entry->Id).second;
            nameInserted = m_ByName.emplace(entry->Descriptor.Name, entry->Id).second;
            nativeInserted = m_ByNative.emplace(entry->Descriptor.NativeKey, entry->Id).second;
            const TypeId id = entry->Id;
            m_All.push_back(entry.get());
            m_Types.push_back(std::move(entry));
            return id;
        }
        catch (const std::exception& exception) {
            if (entry) {
                if (nativeInserted) m_ByNative.erase(entry->Descriptor.NativeKey);
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
    const RegisteredType* TypeRegistry::FindNative(const void* key) const {
        std::shared_lock lock(m_Mutex);
        const auto found = m_ByNative.find(key);
        return found == m_ByNative.end() ? nullptr : m_Types[found->second - 1].get();
    }
    bool TypeRegistry::Owns(const RegisteredType& type) const {
        std::shared_lock lock(m_Mutex);
        return type.Owner == this && type.Id != InvalidTypeId && type.Id <= m_Types.size() &&
            m_Types[type.Id - 1].get() == &type;
    }
}
