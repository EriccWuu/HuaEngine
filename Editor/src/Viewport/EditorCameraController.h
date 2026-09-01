#pragma once

#include <chrono>

#include "HuaEngine/Rendering/RenderCamera.h"

namespace HE::Editor {
	struct EditorCameraInputState {
		float MoveForward = 0.0f;
		float MoveRight = 0.0f;
		float LookX = 0.0f;
		float LookY = 0.0f;
		float PanX = 0.0f;
		float PanY = 0.0f;
		float Zoom = 0.0f;
	};

	class EditorCameraController {
	public:
		EditorCameraController() = default;
		EditorCameraController(float fov, float aspectRatio, float nearClip, float farClip);

		[[nodiscard]] bool Update(const EditorCameraInputState& input);
		[[nodiscard]] Rendering::RenderCamera BuildRenderCamera() const;

		inline void SetViewport(float width, float height) { m_Viewport = { width, height }; }
		const glm::vec3 GetUpDirection() const;
		const glm::vec3 GetRightDirection() const;
		const glm::vec3 GetForwardDirection() const;
		const glm::quat GetOrientation() const;
		[[nodiscard]] const glm::vec3& GetPosition() const { return m_Position; }
		float GetPitch() const { return m_Pitch; }
		float GetYaw() const { return m_Yaw; }
		void SetPose(const glm::vec3& position, float pitch, float yaw);
		void ResetPose();

	private:
		float m_Fov = 45.0f;
		float m_AspectRatio = 16.0f / 9.0f;
		float m_Near = 0.1f, m_Far = 10000.0f;
		float m_Pitch = 0.0f, m_Yaw = 0.0f;

		glm::vec3 m_Position = { 0, 0, 0 };
		glm::vec2 m_Viewport = { 1280, 720 };
		std::chrono::steady_clock::time_point m_LastUpdateTime = std::chrono::steady_clock::now();
		float m_MoveSpeed = 4.0f;
		float m_MouseSensitivity = 0.003f;
		float m_ScrollSpeed = 1.5f;
		float m_PanSpeed = 0.01f;
	};
}
