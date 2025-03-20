#include <iostream>
#include <HuaEngine.h>

class CustomLayer : public HE::Layer {
public:
	CustomLayer(): Layer("CumsomLayer") {

	}

	void OnUpdate() override {

	}

	void OnGuiRender() override {
		ImGui::Begin("Test");
		ImGui::Text("Hello");
		ImGui::End();
	}
};

class SandboxApp : public HE::Application {
public:
	SandboxApp() {
		PushLayer(new CustomLayer());
	}

	~SandboxApp() {

	}
};

HE::Application* HE::CreateApplication() {
	return new SandboxApp();
}