#pragma once
#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Core/Log.h"
#include "Application.h"

#ifdef HE_PLATFORM_WINDOWS
extern HE::Application* HE::CreateApplication();

int main(int argn, char** args) {
	HE::Log::Init();
	auto app = HE::CreateApplication();
	app->Start();
	app->Run();
	delete app;
}
#endif
