#pragma once

#include "HuaEngine/Events/Event.h"
#include "Camera.h"

namespace HE {
	class EditorCamera : public Camera {
	public:
		EditorCamera() = default;
		EditorCamera(float fov, float aspectRatio, float nearClip, float farClip);
		~EditorCamera() = default;

		virtual void OnUpdate() override;
		void OnEvent(Event& event);

		inline void SetViewport(float width, float height) { m_Viewport = { width, height }; }
		const glm::vec3 GetUpDirection() const;
		const glm::vec3 GetRightDirection() const;
		const glm::vec3 GetForwardDirection() const;
		const glm::quat GetOrientation() const;
		inline const glm::vec3 GetPosition() { return m_Position; }
		float GetPitch() const { return m_Pitch; }
		float GetYaw() const { return m_Yaw; }

	private:
		void UpdateProjectionMat();
		void UpdateViewMat();


	private:
		float m_Fov = 45.0f;
		float m_AspectRatio = 16.0f / 9.0f;
		float m_Near = 0.1f, m_Far = 100.0f;
		float m_Pitch = 0.0f, m_Yaw = 0.0f;

		glm::vec3 m_Position = { 0, 0, 0 };
		glm::vec2 m_Viewport = { 1280, 720 };
		// glm::mat4 m_ViewMat;
	};
}