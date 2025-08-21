#include "enginepch.h"
#include "OpenGLContext.h"
#include "glad/glad.h"

namespace HE {
	OpenGLContext::OpenGLContext(GLFWwindow* windowHandle) : m_WindowHandle(windowHandle) {
		HE_CORE_ASSERT(m_WindowHandle, "Window Handle is null!");
	}

	void OpenGLContext::Init()
	{
		glfwMakeContextCurrent(m_WindowHandle);
		int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
		HE_CORE_ASSERT(status, "Failed to initialize Glad!");

		HE_CORE_INFO("OpenGL Rendering Info:");
		HE_CORE_INFO("\t{0}", (const char*)glGetString(GL_VENDOR));
		HE_CORE_INFO("\t{0}", (const char*)glGetString(GL_RENDERER));
		HE_CORE_INFO("\t{0}", (const char*)glGetString(GL_VERSION));
	}

	void OpenGLContext::SwapBuffers()
	{
		glfwSwapBuffers(m_WindowHandle);
	}
}

