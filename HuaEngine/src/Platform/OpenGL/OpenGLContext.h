#pragma once

#include "HuaEngine/Renderer/RenderContext.h"
#include "GLFW/glfw3.h"

namespace HE {
	class OpenGLContext : public RenderContext {
	public:
		OpenGLContext(GLFWwindow* windowHandle);
		virtual void Init() override;
		virtual void SwapBuffers() override;

	private:
		GLFWwindow* m_WindowHandle;
	};
}