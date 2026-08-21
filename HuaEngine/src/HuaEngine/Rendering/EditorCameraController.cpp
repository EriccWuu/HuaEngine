#include "enginepch.h"
#include "EditorCameraController.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/quaternion.hpp"

namespace HE::Rendering {
	EditorCameraController::EditorCameraController(float fov, float aspectRatio, float nearClip, float farClip)
		:m_Fov(fov), m_AspectRatio(aspectRatio), m_Near(nearClip), m_Far(farClip) {}

	void EditorCameraController::OnEvent(Event& event) {
		HE_CORE_WARN("EditorCameraController::OnEvent not implemented!");
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

	RenderCamera EditorCameraController::BuildRenderCamera() const {
		const float aspectRatio = m_Viewport.y > 0.0f ? m_Viewport.x / m_Viewport.y : m_AspectRatio;
		const auto projection = glm::perspective(glm::radians(m_Fov), aspectRatio, m_Near, m_Far);
		const auto view = glm::inverse(glm::translate(glm::mat4(1.0f), m_Position) * glm::toMat4(GetOrientation()));
		return RenderCamera(projection, view);
	}
}
