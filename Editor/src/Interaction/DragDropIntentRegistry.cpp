#include "enginepch.h"
#include "Interaction/DragDropIntentRegistry.h"

namespace HE {
    void DragDropIntentRegistry::Clear() {
        m_Intents.clear();
    }

    void DragDropIntentRegistry::Register(DragDropIntentDescriptor descriptor) {
        m_Intents.push_back(std::move(descriptor));
    }

    const DragDropIntentDescriptor* DragDropIntentRegistry::Find(std::string_view source, std::string_view target) const {
        for (const auto& intent : m_Intents) {
            if (intent.Source == source && intent.Target == target) {
                return &intent;
            }
        }

        return nullptr;
    }
}
