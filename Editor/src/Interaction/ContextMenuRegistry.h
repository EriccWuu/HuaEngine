#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace HE {
    struct ContextMenuActionDescriptor {
        std::string Id;
        std::string Label;
        std::string Shortcut;
        std::string Tooltip;
        bool Enabled = true;
        std::function<bool()> IsEnabled;
        std::function<void()> Trigger;
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
