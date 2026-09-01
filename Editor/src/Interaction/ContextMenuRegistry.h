#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace HE {
    struct ContextMenuActionDescriptor {
        std::string CommandId;
        std::string Label;
        std::string Tooltip;
        bool Enabled = true;
    };

    class ContextMenuRegistry {
    public:
        void Clear();
        void Register(std::string_view contextId, ContextMenuActionDescriptor descriptor);
        void Replace(std::string_view contextId, std::vector<ContextMenuActionDescriptor> descriptors);

        [[nodiscard]] const std::vector<ContextMenuActionDescriptor>* Find(std::string_view contextId) const;

    private:
        std::unordered_map<std::string, std::vector<ContextMenuActionDescriptor>> m_ActionsByContext;
    };
}
