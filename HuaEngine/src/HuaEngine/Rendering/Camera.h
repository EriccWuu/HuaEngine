#pragma once

#include "glm/glm.hpp"

namespace HE::Rendering {
	class Camera {
	public:
		Camera() = default;
		Camera(const Camera& other) = default;
		Camera(const glm::mat4& projection): m_ProjectionMat(projection) {}
		virtual ~Camera() = default;

		virtual void OnUpdate() {};

		const glm::mat4& GetProjection() const { return m_ProjectionMat; }
		const glm::mat4& GetView() const { return m_ViewMat; }
		const glm::mat4& GetViewProjection() const { return m_ProjectionMat * m_ViewMat; }

	protected:
		glm::mat4 m_ProjectionMat = glm::mat4(1.0f);
		glm::mat4 m_ViewMat = glm::mat4(1.0f);
	};
}