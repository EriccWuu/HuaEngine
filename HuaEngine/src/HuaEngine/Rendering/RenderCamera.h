#pragma once

#include "glm/glm.hpp"

namespace HE::Rendering {
	class RenderCamera {
	public:
		RenderCamera() = default;
		RenderCamera(const glm::mat4& projection, const glm::mat4& view)
			: m_ProjectionMat(projection), m_ViewMat(view) {}

		[[nodiscard]] const glm::mat4& GetProjection() const { return m_ProjectionMat; }
		[[nodiscard]] const glm::mat4& GetView() const { return m_ViewMat; }
		[[nodiscard]] glm::mat4 GetViewProjection() const { return m_ProjectionMat * m_ViewMat; }

		void SetProjection(const glm::mat4& projection) { m_ProjectionMat = projection; }
		void SetView(const glm::mat4& view) { m_ViewMat = view; }

	protected:
		glm::mat4 m_ProjectionMat = glm::mat4(1.0f);
		glm::mat4 m_ViewMat = glm::mat4(1.0f);
	};
}
