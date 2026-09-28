#include "HuaEngine/ECS/Runtime/Resources.h"

#include <algorithm>
#include <limits>
#include <mutex>

namespace HE::Ecs {
    Result<ResourceHandle> ResourceRegistry::RegisterObject(std::shared_ptr<void> object, const void* nativeKey) {
        if (std::this_thread::get_id() != m_OwnerThread) {
            return Error{ErrorCode::WrongThread, "RegisterResource", "Resources must be registered on their owner thread"};
        }
        if (!object || !nativeKey) {
            return Error{ErrorCode::InvalidArgument, "RegisterResource", "A resource must own a non-null object"};
        }
        std::unique_lock lock(m_Mutex);
        if (const auto found = m_ByAddress.find(object.get()); found != m_ByAddress.end()) {
            const auto& entry = *m_Entries[static_cast<size_t>(found->second - 1)];
            if (entry.NativeKey != nativeKey) {
                return Error{ErrorCode::InvalidType, "RegisterResource", "An object identity cannot have two native types"};
            }
            return ResourceHandle{entry.Id, this};
        }
        try {
            if (m_Entries.size() == std::numeric_limits<ResourceId>::max()) {
                return Error{ErrorCode::InvalidState, "RegisterResource", "Resource identity space is exhausted"};
            }
            const ResourceId id = static_cast<ResourceId>(m_Entries.size()) + 1;
            auto entry = std::make_unique<RegisteredResource>(RegisteredResource{id, nativeKey, std::move(object)});
            if (m_Entries.size() == m_Entries.capacity()) m_Entries.reserve(std::max<size_t>(8, m_Entries.size() * 2));
            m_ByAddress.emplace(entry->Object.get(), id);
            m_Entries.push_back(std::move(entry));
            return ResourceHandle{id, this};
        }
        catch (const std::exception& exception) {
            return Error{ErrorCode::ConstructionFailed, "RegisterResource", exception.what()};
        }
    }

    const RegisteredResource* ResourceRegistry::Find(ResourceHandle handle) const {
        if (handle.Owner != this || handle.Id == 0) return nullptr;
        std::shared_lock lock(m_Mutex);
        if (handle.Id > m_Entries.size()) return nullptr;
        return m_Entries[static_cast<size_t>(handle.Id - 1)].get();
    }
}
