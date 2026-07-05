#include "enginepch.h"
#include "OpenGLRenderDevice.h"

#include "glad/glad.h"

#include "HuaEngine/Rendering/Camera.h"
#include "HuaEngine/Rendering/FrameBuffer.h"
#include "HuaEngine/Rendering/Material/Material.h"
#include "HuaEngine/Rendering/VertexArray.h"

namespace HE::Rendering {
	void OpenGLCommandList::BeginRenderTarget(FrameBuffer& target) {
		m_CurrentTarget = &target;
		m_CurrentTarget->Bind();
	}

	void OpenGLCommandList::ClearColor(const glm::vec4& color) {
		glClearColor(color.r, color.g, color.b, color.a);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	void OpenGLCommandList::BeginFrame(Camera& camera) {
		m_CurrentCamera = &camera;
	}

	void OpenGLCommandList::DrawIndexed(MaterialInstance& material, VertexArray& vertexArray, const glm::mat4& transform) {
		if (!m_CurrentCamera || !material.GetShader()) {
			HE_CORE_WARN("Trying to draw without a camera or material shader");
			return;
		}

		const auto& shader = material.GetShader();
		shader->SetMat4("u_ViewProjection", m_CurrentCamera->GetViewProjection());
		shader->SetMat4("u_Transform", transform);

		material.Bind();
		vertexArray.Bind();

		const auto& indexBuffer = vertexArray.GetIndexBuffer();
		if (!indexBuffer) {
			HE_CORE_WARN("Trying to draw vertex array without an index buffer");
			material.Unbind();
			return;
		}

		glDrawElements(GL_TRIANGLES, indexBuffer->GetCount(), GL_UNSIGNED_INT, nullptr);
		material.Unbind();
	}

	void OpenGLCommandList::EndFrame() {
		m_CurrentCamera = nullptr;
	}

	void OpenGLCommandList::EndRenderTarget() {
		if (m_CurrentTarget) {
			m_CurrentTarget->Unbind();
			m_CurrentTarget = nullptr;
		}
	}

	OpenGLRenderDevice::OpenGLRenderDevice() {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	CommandList& OpenGLRenderDevice::GetImmediateCommandList() {
		return m_ImmediateCommandList;
	}
}
