#pragma once

#include <chrono>

#include "HuaEngine/Events/Event.h"
#include "HuaEngine/Rendering/RenderCamera.h"

namespace HE::Editor {
	class EditorCameraController {
	public:
		EditorCameraController() = default;
		EditorCameraController(float fov, float aspectRatio, float nearClip, float farClip);

		void OnEvent(Event& event);
		void Update(bool isActive);
		[[nodiscard]] Rendering::RenderCamera BuildRenderCamera() const;

		inline void SetViewport(float width, float height) { m_Viewport = { width, height }; }
		const glm::vec3 GetUpDirection() const;
		const glm::vec3 GetRightDirection() const;
		const glm::vec3 GetForwardDirection() const;
		const glm::quat GetOrientation() const;
		inline const glm::vec3 GetPosition() { return m_Position; }
		float GetPitch() const { return m_Pitch; }
		float GetYaw() const { return m_Yaw; }

	private:
		float m_Fov = 45.0f;
		float m_AspectRatio = 16.0f / 9.0f;
		float m_Near = 0.1f, m_Far = 100.0f;
		float m_Pitch = 0.0f, m_Yaw = 0.0f;

		glm::vec3 m_Position = { 0, 0, 0 };
		glm::vec2 m_Viewport = { 1280, 720 };
		glm::vec2 m_LastMousePosition = { 0, 0 };
		std::chrono::steady_clock::time_point m_LastUpdateTime = std::chrono::steady_clock::now();
		bool m_HasMousePosition = false;
		float m_MoveSpeed = 4.0f;
		float m_MouseSensitivity = 0.003f;
		float m_ScrollSpeed = 1.5f;
	};
}
