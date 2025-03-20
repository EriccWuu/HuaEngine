#pragma once

#include "HuaEngine/Core/Layer.h"
#include "HuaEngine/Events/ApplicationEvent.h"
#include "HuaEngine/Events/KeyEvent.h"
#include "HuaEngine/Events/MouseEvent.h"

namespace HE {
	class ENGINE_API ImguiLayer : public Layer {
	public:
		ImguiLayer(): Layer("ImGuiLayer") {}
		~ImguiLayer() {}

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnGuiRender() override;
		void Begin();
		void End();
	};
}