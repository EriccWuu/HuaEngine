#include <iostream>
#include <HuaEngine.h>

#include "HuaEngine/Platform/OpenGL/OpenGLShader.h"
#include "HuaEngine/Platform/OpenGL/OpenGLTexture2D.h"

using namespace HE;

class CustomLayer : public HE::Layer {
public:
	CustomLayer(): Layer("CumsomLayer") {
		float squareVertices[4 * 5] = {
			-0.8f, -0.8f, 0.0f, 0.0 , 0.0,
			 0.8f, -0.8f, 0.0f, 1.0 , 0.0,
			 0.8f,  0.8f, 0.0f, 1.0 , 1.0,
			-0.8f,  0.8f, 0.0f, 0.0 , 1.0
		};

		unsigned int squareIndices[6] = {
			0, 1, 2, 2, 3, 0
		};

		m_SquareVA = VertexArray::Create();

		Ref<VertexBuffer> squareVertexBuffer;
		squareVertexBuffer = VertexBuffer::Create(squareVertices, sizeof(squareVertices));
		Ref<IndexBuffer> squareIndexBuffer;
		squareIndexBuffer = IndexBuffer::Create(squareIndices, sizeof(squareIndices) / sizeof(uint32_t));

		BufferLayout squareLayout = {
			{ ShaderDataType::Float3, "aPosition" },
			{ ShaderDataType::Float2, "aTexCoord" }
		};

		squareVertexBuffer->SetLayout(squareLayout);

		m_SquareVA->AddVertexBuffer(squareVertexBuffer);
		m_SquareVA->SetIndexBuffer(squareIndexBuffer);

		std::string squareVS = R"(
			#version 330 core

			layout(location = 0) in vec3 aPosition;
			layout(location = 1) in vec2 aTexCoord;

			out vec3 vPosition;
			out vec2 vTexCoord;

			void main() {
				vPosition = aPosition;
				vTexCoord = aTexCoord;
				gl_Position = vec4(aPosition, 1.0);
			}
		)";

		std::string squareFS = R"(
			#version 330 core

			out vec4 FragColor;
			in vec3 vPosition;
			in vec2 vTexCoord;

			uniform sampler2D uTexture;

			void main() {
				FragColor = texture(uTexture, vTexCoord);
			}
		)";

		m_SquareShader.reset(new OpenGLShader(squareVS, squareFS));

		m_Texture = Texture2D::Create("assets/textures/hutao.png");
		m_Texture->Bind(0);
		std::dynamic_pointer_cast<OpenGLShader>(m_SquareShader)->UploadUniformInt("uTexture", 0);
	}

	void OnUpdate() override {
		Renderer::Begin();

		m_SquareShader->Bind();
		Renderer::Submit(m_SquareVA);

		Renderer::End();
	}

private:
	Ref<Shader> m_SquareShader;
	Ref<VertexArray> m_SquareVA;
	Ref<Texture2D> m_Texture;
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