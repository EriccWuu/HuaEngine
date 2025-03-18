#pragma once
#include "Core.h"

namespace HE
{
	class ENGINE_API Application
	{
	public :
		Application();

		~Application();

		void Run();
	};

	Application* CreateApplication();
}

