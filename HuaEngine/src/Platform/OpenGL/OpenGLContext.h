#pragma once

#include "HuaEngine/Rendering/RenderContext.h"
#include "GLFW/glfw3.h"

namespace HE {
	class OpenGLContext : public Rendering::RenderContext {
	public:
		OpenGLContext(GLFWwindow* windowHandle);
		virtual void Init() override;
		virtual void SwapBuffers() override;

	private:
		GLFWwindow* m_WindowHandle;
	};
}