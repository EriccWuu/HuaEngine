#include "enginepch.h"
#include "Renderer.h"
#include "RenderCommand.h"

namespace HE {
	Ref<Camera> Renderer::m_Camera = nullptr;

	void Renderer::Init() {
		RenderCommand::Init();
	}

	void Renderer::Begin(Ref<Camera> camera) {
		m_Camera = camera;
	}

	void Renderer::End() {

	}

	void Renderer::Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform) {
		shader->SetMat4("u_ViewProjection", m_Camera->GetViewProjection());
		shader->SetMat4("u_Transform", transform);

		vertexArray->Bind();
		RenderCommand::DrawIndexed(vertexArray);
	}

	void Renderer::Submit(const Ref<MaterialInstance>& materialInstance, const Ref<VertexArray>& vertexArray, const glm::mat4& transform) {
		if (!materialInstance || !materialInstance->GetShader()) {
			HE_CORE_WARN("Trying to submit with null material instance or shader");
			return;
		}

		auto shader = materialInstance->GetShader();
		
		// 设置标准矩阵 uniforms
		shader->SetMat4("u_ViewProjection", m_Camera->GetViewProjection());
		shader->SetMat4("u_Transform", transform);

		// 绑定材质并应用参数
		materialInstance->Bind();

		vertexArray->Bind();
		RenderCommand::DrawIndexed(vertexArray);

		materialInstance->Unbind();
	}

}