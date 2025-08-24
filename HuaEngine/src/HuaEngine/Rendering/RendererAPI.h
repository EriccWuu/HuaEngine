#pragma once
#include "VertexArray.h"
#include "glm/glm.hpp"

namespace HE::Rendering {
	class RendererAPI {
	public:
		enum class API {
			None = 0,
			OpenGL = 1
		};

		inline static API GetAPI() { return m_API; }
		static RendererAPI* Create();

		virtual ~RendererAPI() = default;
		virtual void Init() = 0;
		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray) = 0;
		virtual void SetClearColor(const glm::vec4& clearColor) = 0;
		virtual void SetViewport(const float width, const float height) = 0;
		virtual void Clear() = 0;

	private:
		static API m_API;
	};
}