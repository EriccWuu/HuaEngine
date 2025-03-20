#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Events/Event.h"

namespace HE {
    class ENGINE_API Layer {
    public:
        Layer(const std::string& name = "Layer");
        virtual ~Layer() = default;

        virtual void OnAttach() {}
        virtual void OnDetach() {}
        virtual void OnUpdate() {}
        virtual void OnEvent(Event& event) {}
        virtual void OnGuiRender() {}

        inline const std::string& GetName() const { return m_DebugName; }
    private:
        std::string m_DebugName;
    };
}