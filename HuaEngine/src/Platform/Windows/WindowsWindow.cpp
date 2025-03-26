#include "enginepch.h"
#include "Platform/Windows/WindowsWindow.h"
#include "Platform/OpenGL/OpenGLContext.h"
#include "Platform/OpenGL/OpenGLShader.h"

#include "HuaEngine/Events/ApplicationEvent.h"
#include "HuaEngine/Events/MouseEvent.h"
#include "HuaEngine/Events/KeyEvent.h"

namespace HE {
	unsigned int shaderProgram;
	
	static uint8_t ms_GLFWWindowCount = 0;
	static bool ms_GLFWInitialized = false;

	static void GLFWErrorCallback(int error, const char* description)
	{
		HE_CORE_ERROR("GLFW Error ({0}): {1}", error, description);
	}

	WindowsWindow::WindowsWindow(const WindowProps& props)
	{
		Init(props);
	}

	WindowsWindow::~WindowsWindow()
	{
		Shutdown();
	}

	void WindowsWindow::Init(const WindowProps& props)
	{
		m_Data.Title = props.Title;
		m_Data.Width = props.Width;
		m_Data.Height = props.Height;

		HE_CORE_INFO("Creating window {0} ({1}, {2})", props.Title, props.Width, props.Height);

		if (!ms_GLFWInitialized) {
			int success = glfwInit();
			HE_CORE_ASSERT(success, "Could not initialize GLFW!");
			glfwSetErrorCallback(GLFWErrorCallback);
			ms_GLFWInitialized = true;
		}

		m_Window = glfwCreateWindow((int)props.Width, (int)props.Height, m_Data.Title.c_str(), nullptr, nullptr);

		m_Context = new OpenGLContext(m_Window);
		m_Context->Init();

		glfwSetWindowUserPointer(m_Window, &m_Data);
		SetVSync(true);

		// Window Events
		glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height) {
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			data.Width = width;
			data.Height = height;
			auto event = WindowResizeEvent(width, height);
			data.EventCallback(event);
		});

		glfwSetWindowPosCallback(m_Window, [](GLFWwindow* window, int xpos, int ypos) {
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			auto event = WindowMovedEvent();
			data.EventCallback(event);
		});

		glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window) {
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			auto event = WindowCloseEvent();
			data.EventCallback(event);
		});

		glfwSetWindowFocusCallback(m_Window, [](GLFWwindow* window, int focused) {
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			switch (focused) {
				case GLFW_TRUE: {
					auto event = WindowFocusEvent();
					data.EventCallback(event);
					break;
				}
				case GLFW_FALSE: {
					auto event = WindowLostFocusEvent();
					data.EventCallback(event);
					break;
				}
			}
		});

		// Mouse Events
		glfwSetMouseButtonCallback(m_Window, [](GLFWwindow *window, int button, int action, int mods) {
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			switch (action) {
				case GLFW_PRESS: {
					auto event = MouseButtonPressedEvent(button);
					data.EventCallback(event);
					break;
				}
				case GLFW_RELEASE: {
					auto event = MouseButtonReleasedEvent(button);
					data.EventCallback(event);
					break;
				}
			}
		});

		glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xpos, double ypos) {
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			auto event = MouseMovedEvent((float)xpos, (float)ypos);
			data.EventCallback(event);
		});

		glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xoffset, double yoffset) {
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			auto event = MouseScrolledEvent((float)xoffset, (float)yoffset);
			data.EventCallback(event);
		});

		// Keyboard Events
		glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			switch (action) {
				case GLFW_PRESS: {
					auto event = KeyPressedEvent(key, false);
					data.EventCallback(event);
					break;
				}
				case GLFW_RELEASE: {
					auto event = KeyReleasedEvent(key);
					data.EventCallback(event);
					break;
				}
				case GLFW_REPEAT: {
					auto event = KeyPressedEvent(key, true);
					data.EventCallback(event);
					break;
				}
			}
		});

		glfwSetCharCallback(m_Window, [](GLFWwindow* window, unsigned int codepoint) {
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			auto event = KeyTypedEvent(codepoint);
			data.EventCallback(event);
		});
	}

	void WindowsWindow::Shutdown()
	{
		glfwDestroyWindow(m_Window);
		--ms_GLFWWindowCount;

		if (ms_GLFWWindowCount == 0) 
		{
			glfwTerminate();
		}
	}

	void WindowsWindow::OnUpdate()
	{
		glfwPollEvents();
		m_Context->SwapBuffers();
	}

	void WindowsWindow::SetVSync(bool enabled)
	{
		if (enabled)
			glfwSwapInterval(1);
		else
			glfwSwapInterval(0);

		m_Data.VSync = enabled;
	}

	bool WindowsWindow::IsVSync() const
	{
		return m_Data.VSync;
	}
}