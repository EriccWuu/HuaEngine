#include "enginepch.h"
#include "Window.h"

#ifdef HE_PLATFORM_WINDOWS
	#include "Platform/Windows/WindowsWindow.h"
#endif

namespace HE
{
	Window* Window::Create(const WindowProps& props)
	{
	#ifdef HE_PLATFORM_WINDOWS
		return new WindowsWindow(props);
	#else
		HE_CORE_ASSERT(false, "Unknown platform!");
		return nullptr;
	#endif
	}
}