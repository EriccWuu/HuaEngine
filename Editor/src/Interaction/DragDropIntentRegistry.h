#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace HE {
    struct DragDropIntentDescriptor {
        std::string Id;
        std::string Label;
        std::string PayloadType;
        std::string Source;
        std::string Target;
        bool Enabled = true;
    };

    class DragDropIntentRegistry {
    public:
        void Clear();
        void Register(DragDropIntentDescriptor descriptor);
        [[nodiscard]] const DragDropIntentDescriptor* Find(std::string_view source, std::string_view target) const;

        [[nodiscard]] const std::vector<DragDropIntentDescriptor>& GetIntents() const { return m_Intents; }

    private:
        std::vector<DragDropIntentDescriptor> m_Intents;
    };
}
