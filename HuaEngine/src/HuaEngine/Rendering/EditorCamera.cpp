#include "enginepch.h"
#include "EditorCamera.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/quaternion.hpp"

namespace HE {
	EditorCamera::EditorCamera(float fov, float aspectRatio, float nearClip, float farClip)
		:m_Fov(fov), m_AspectRatio(aspectRatio), m_Near(nearClip), m_Far(farClip) {
		UpdateViewMat();
	}

	void EditorCamera::OnUpdate() {
		UpdateProjectionMat();
		UpdateViewMat();
	}

	void EditorCamera::OnEvent(Event& event) {
		HE_CORE_WARN("EditorCamera::OnEvent not implemented!");
	}

	const glm::vec3 EditorCamera::GetUpDirection() const {
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 1.0f, 0.0f));
	}

	const glm::vec3 EditorCamera::GetRightDirection() const {
		return glm::rotate(GetOrientation(), glm::vec3(1.0f, 0.0f, 0.0f));
	}

	const glm::vec3 EditorCamera::GetForwardDirection() const {
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 0.0f, -1.0f));
	}

	const glm::quat EditorCamera::GetOrientation() const {
		return glm::quat(glm::vec3(-m_Pitch, -m_Yaw, 0.0f));
	}

	void EditorCamera::UpdateProjectionMat() {
		m_AspectRatio = m_Viewport.x / m_Viewport.y;
		m_ProjectionMat = glm::perspective(glm::radians(m_Fov), m_AspectRatio, m_Near, m_Far);
	}

	void EditorCamera::UpdateViewMat() {
		glm::quat orientation = GetOrientation();
		m_ViewMat = glm::translate(glm::mat4(1.0), m_Position) * glm::toMat4(orientation);
		m_ViewMat = glm::inverse(m_ViewMat);
	}
}