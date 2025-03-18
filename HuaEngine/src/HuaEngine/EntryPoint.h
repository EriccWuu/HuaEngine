#pragma once

#ifdef HE_PLATFORM_WINDOWS
extern HE::Application* HE::CreateApplication();

int main(int argn, char** args) {
	auto app = HE::CreateApplication();
	HE::Log::Init();
	HE_CORE_WARN("Initialized Log!");
	int a = 5;
	HE_INFO("hello Var = {0}", a);
	app->Run();
	delete app;
}
#endif