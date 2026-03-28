#include "enginepch.h"
#include "Interaction/ContextMenuRegistry.h"

namespace HE {
    void ContextMenuRegistry::Clear() {
        m_ActionsByContext.clear();
    }

    void ContextMenuRegistry::Register(std::string_view contextId, ContextMenuActionDescriptor descriptor) {
        m_ActionsByContext[std::string(contextId)].push_back(std::move(descriptor));
    }

    void ContextMenuRegistry::Replace(std::string_view contextId, std::vector<ContextMenuActionDescriptor> descriptors) {
        m_ActionsByContext[std::string(contextId)] = std::move(descriptors);
    }

    const std::vector<ContextMenuActionDescriptor>* ContextMenuRegistry::Find(std::string_view contextId) const {
        const auto it = m_ActionsByContext.find(std::string(contextId));
        return it != m_ActionsByContext.end() ? &it->second : nullptr;
    }
}
