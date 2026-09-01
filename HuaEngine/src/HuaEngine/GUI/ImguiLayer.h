#pragma once

#include "HuaEngine/Core/Layer.h"
#include "HuaEngine/Events/ApplicationEvent.h"

namespace HE {
	class InputSystem;
	class ENGINE_API ImguiLayer : public Layer {
	public:
		ImguiLayer(): Layer("ImGuiLayer") {}
		~ImguiLayer() {}

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnGuiRender() override;
		void Begin(InputSystem* inputSystem = nullptr);
		void End();
	};
}
