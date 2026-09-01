#include "enginepch.h"
#include "EditorCameraController.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/quaternion.hpp"

namespace HE::Editor {
	EditorCameraController::EditorCameraController(float fov, float aspectRatio, float nearClip, float farClip)
		:m_Fov(fov), m_AspectRatio(aspectRatio), m_Near(nearClip), m_Far(farClip) {}

	bool EditorCameraController::Update(const EditorCameraInputState& input) {
		const auto now = std::chrono::steady_clock::now();
		const float deltaTime = std::chrono::duration<float>(now - m_LastUpdateTime).count();
		m_LastUpdateTime = now;

		const float moveDistance = m_MoveSpeed * deltaTime;
		m_Position += GetForwardDirection() * input.MoveForward * moveDistance;
		m_Position += GetRightDirection() * input.MoveRight * moveDistance;
		m_Yaw += input.LookX * m_MouseSensitivity;
		m_Pitch = glm::clamp(m_Pitch + input.LookY * m_MouseSensitivity, -1.55f, 1.55f);
		m_Position -= GetRightDirection() * input.PanX * m_PanSpeed;
		m_Position += GetUpDirection() * input.PanY * m_PanSpeed;
		m_Position += GetForwardDirection() * input.Zoom * m_ScrollSpeed;
		const bool changed = input.MoveForward != 0.0f || input.MoveRight != 0.0f ||
			input.LookX != 0.0f || input.LookY != 0.0f || input.PanX != 0.0f ||
			input.PanY != 0.0f || input.Zoom != 0.0f;
		return changed;
	}

	void EditorCameraController::SetPose(const glm::vec3& position, float pitch, float yaw) {
		m_Position = position;
		m_Pitch = glm::clamp(pitch, -1.55f, 1.55f);
		m_Yaw = yaw;
	}

	void EditorCameraController::ResetPose() {
		SetPose(glm::vec3(0.0f), 0.0f, 0.0f);
	}

	const glm::vec3 EditorCameraController::GetUpDirection() const {
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 1.0f, 0.0f));
	}

	const glm::vec3 EditorCameraController::GetRightDirection() const {
		return glm::rotate(GetOrientation(), glm::vec3(1.0f, 0.0f, 0.0f));
	}

	const glm::vec3 EditorCameraController::GetForwardDirection() const {
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 0.0f, -1.0f));
	}

	const glm::quat EditorCameraController::GetOrientation() const {
		return glm::quat(glm::vec3(-m_Pitch, -m_Yaw, 0.0f));
	}

	Rendering::RenderCamera EditorCameraController::BuildRenderCamera() const {
		const float aspectRatio = m_Viewport.y > 0.0f ? m_Viewport.x / m_Viewport.y : m_AspectRatio;
		const auto projection = glm::perspective(glm::radians(m_Fov), aspectRatio, m_Near, m_Far);
		const auto view = glm::inverse(glm::translate(glm::mat4(1.0f), m_Position) * glm::toMat4(GetOrientation()));
		return Rendering::RenderCamera(projection, view);
	}
}
