#include <iostream>
#include <HuaEngine.h>

#include "HuaEngine/Platform/OpenGL/OpenGLShader.h"

using namespace HE;

class CustomLayer : public HE::Layer {
public:
	CustomLayer(): Layer("CumsomLayer") {
		float vertices[3 * 7] = {
			-0.5f, -0.5f, 0.0f, 1.f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
			0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f
		};

		unsigned int indices[3] = {
			0, 1, 2
		};

		m_VertexArray.reset(VertexArray::Create());

		std::shared_ptr<VertexBuffer> vertexBuffer;
		vertexBuffer.reset(VertexBuffer::Create(vertices, sizeof(vertices)));
		std::shared_ptr<IndexBuffer> indexBuffer;
		indexBuffer.reset(IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));

		BufferLayout layout = {
		{ ShaderDataType::Float3, "aPos" },
		{ ShaderDataType::Float4, "aColor" }
		};

		vertexBuffer->SetLayout(layout);

		m_VertexArray->AddVertexBuffer(vertexBuffer);
		m_VertexArray->SetIndexBuffer(indexBuffer);

		std::string vertexSource = R"(
			#version 330 core

			layout(location = 0) in vec3 aPos;
			layout(location = 1) in vec4 aColor;

			out vec3 vPos;
			out vec4 vColor;

			void main() {
				vPos = aPos;
				vColor = aColor;
				gl_Position = vec4(aPos, 1.0);
			}
		)";

		std::string fragmentSource = R"(
			#version 330 core

			out vec4 FragColor;
			in vec3 vPos;
			in vec4 vColor;

			void main() {
				FragColor = vec4(vPos + 0.5, 1.0);
				FragColor = vColor;
			}
		)";

		m_Shader.reset(new OpenGLShader(vertexSource, fragmentSource));

		float squareVertices[4 * 3] = {
			-0.8f, -0.8f, 0.0f,
			 0.8f, -0.8f, 0.0f,
			 0.8f,  0.8f, 0.0f,
			-0.8f,  0.8f, 0.0f
		};

		unsigned int squareIndices[6] = {
			0, 1, 2, 2, 3, 0
		};

		m_SquareVA.reset(VertexArray::Create());

		std::shared_ptr<VertexBuffer> squareVertexBuffer;
		squareVertexBuffer.reset(VertexBuffer::Create(squareVertices, sizeof(squareVertices)));
		std::shared_ptr<IndexBuffer> squareIndexBuffer;
		squareIndexBuffer.reset(IndexBuffer::Create(squareIndices, sizeof(squareIndices) / sizeof(uint32_t)));

		BufferLayout squareLayout = {
			{ ShaderDataType::Float3, "aPos" }
		};

		squareVertexBuffer->SetLayout(squareLayout);

		m_SquareVA->AddVertexBuffer(squareVertexBuffer);
		m_SquareVA->SetIndexBuffer(squareIndexBuffer);

		std::string squareVS = R"(
			#version 330 core

			layout(location = 0) in vec3 aPos;

			out vec3 vPos;

			void main() {
				vPos = aPos;
				gl_Position = vec4(aPos, 1.0);
			}
		)";

		std::string squareFS = R"(
			#version 330 core

			out vec4 FragColor;
			in vec3 vPos;

			void main() {
				FragColor = vec4(0.2, 0.3, 0.8, 1.0);
			}
		)";

		m_SquareShader.reset(new OpenGLShader(squareVS, squareFS));
	}

	void OnUpdate() override {
		Renderer::Begin();

		m_SquareShader->Bind();
		Renderer::Submit(m_SquareVA);

		m_Shader->Bind();
		Renderer::Submit(m_VertexArray);

		Renderer::End();
	}

private:
	std::shared_ptr<Shader> m_Shader;
	std::shared_ptr<Shader> m_SquareShader;
	std::shared_ptr<VertexArray> m_VertexArray, m_SquareVA;
};

class SandboxApp : public HE::Application {
public:
	SandboxApp() {
		PushLayer(new CustomLayer());
	}

	~SandboxApp() {

	}
};

HE::Application* HE::CreateApplication() {
	return new SandboxApp();
}