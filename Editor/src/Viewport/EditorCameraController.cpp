#include "enginepch.h"
#include "EditorCameraController.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/quaternion.hpp"
#include "HuaEngine/Core/Input.h"
#include "HuaEngine/Core/KeyCodes.h"
#include "HuaEngine/Core/MouseCodes.h"
#include "HuaEngine/Events/MouseEvent.h"

namespace HE::Editor {
	EditorCameraController::EditorCameraController(float fov, float aspectRatio, float nearClip, float farClip)
		:m_Fov(fov), m_AspectRatio(aspectRatio), m_Near(nearClip), m_Far(farClip) {}

	bool EditorCameraController::OnEvent(Event& event) {
		bool changed = false;
		EventDispatcher dispatcher(event);
		dispatcher.Dispatch<MouseScrolledEvent>([this, &changed](MouseScrolledEvent& mouseEvent) {
			m_Position += GetForwardDirection() * mouseEvent.GetYOffset() * m_ScrollSpeed;
			changed = mouseEvent.GetYOffset() != 0.0f;
			return false;
		});
		return changed;
	}

	bool EditorCameraController::Update(bool isActive) {
		const auto now = std::chrono::steady_clock::now();
		const float deltaTime = std::chrono::duration<float>(now - m_LastUpdateTime).count();
		m_LastUpdateTime = now;

		const glm::vec2 mousePosition = { Input::GetMouseX(), Input::GetMouseY() };
		bool changed = false;
		if (isActive) {
			const float moveDistance = m_MoveSpeed * deltaTime;
			if (Input::IsKeyPressed(Key::W)) { m_Position += GetForwardDirection() * moveDistance; changed = true; }
			if (Input::IsKeyPressed(Key::S)) { m_Position -= GetForwardDirection() * moveDistance; changed = true; }
			if (Input::IsKeyPressed(Key::D)) { m_Position += GetRightDirection() * moveDistance; changed = true; }
			if (Input::IsKeyPressed(Key::A)) { m_Position -= GetRightDirection() * moveDistance; changed = true; }
		}

		if (isActive && Input::IsMousePressed(Mouse::ButtonRight)) {
			if (m_HasMousePosition) {
				const glm::vec2 delta = mousePosition - m_LastMousePosition;
				m_Yaw += delta.x * m_MouseSensitivity;
				m_Pitch = glm::clamp(m_Pitch + delta.y * m_MouseSensitivity, -1.55f, 1.55f);
				changed = delta.x != 0.0f || delta.y != 0.0f;
			}
			m_HasMousePosition = true;
		} else if (isActive && Input::IsMousePressed(Mouse::ButtonMiddle)) {
			if (m_HasMousePosition) {
				const glm::vec2 delta = mousePosition - m_LastMousePosition;
				m_Position -= GetRightDirection() * delta.x * m_PanSpeed;
				m_Position += GetUpDirection() * delta.y * m_PanSpeed;
				changed = delta.x != 0.0f || delta.y != 0.0f;
			}
			m_HasMousePosition = true;
		} else {
			m_HasMousePosition = false;
		}
		m_LastMousePosition = mousePosition;
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
