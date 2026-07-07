#include "enginepch.h"
#include "OpenGLRenderDevice.h"

#include <filesystem>

#include "glad/glad.h"

#include "HuaEngine/Rendering/Camera.h"
#include "HuaEngine/Rendering/FrameBuffer.h"
#include "HuaEngine/Rendering/Material/MaterialBinding.h"
#include "HuaEngine/Rendering/Material/Material.h"
#include "HuaEngine/Rendering/RHI/RenderTarget.h"
#include "HuaEngine/Rendering/RHI/ShaderProgram.h"
#include "HuaEngine/Rendering/RHI/TextureResource.h"
#include "HuaEngine/Rendering/RHI/VertexBufferView.h"
#include "HuaEngine/Rendering/VertexArray.h"
#include "Platform/OpenGL/OpenGLFrameBuffer.h"
#include "Platform/OpenGL/OpenGLShader.h"
#include "Platform/OpenGL/OpenGLTexture2D.h"

namespace {
	GLenum ToOpenGLBufferTarget(HE::Rendering::GpuBufferUsage usage) {
		switch (usage) {
			case HE::Rendering::GpuBufferUsage::Vertex:
				return GL_ARRAY_BUFFER;
			case HE::Rendering::GpuBufferUsage::Index:
				return GL_ELEMENT_ARRAY_BUFFER;
			case HE::Rendering::GpuBufferUsage::Uniform:
				return GL_UNIFORM_BUFFER;
			case HE::Rendering::GpuBufferUsage::Storage:
				return GL_SHADER_STORAGE_BUFFER;
		}

		HE_CORE_ASSERT(false, "Unknown GPU buffer usage");
		return GL_ARRAY_BUFFER;
	}

	GLenum ToOpenGLType(HE::Rendering::ShaderDataType dataType) {
		switch (dataType) {
			case HE::Rendering::ShaderDataType::Float:
			case HE::Rendering::ShaderDataType::Float2:
			case HE::Rendering::ShaderDataType::Float3:
			case HE::Rendering::ShaderDataType::Float4:
			case HE::Rendering::ShaderDataType::Mat3:
			case HE::Rendering::ShaderDataType::Mat4:
				return GL_FLOAT;
			case HE::Rendering::ShaderDataType::Int:
			case HE::Rendering::ShaderDataType::Int2:
			case HE::Rendering::ShaderDataType::Int3:
			case HE::Rendering::ShaderDataType::Int4:
				return GL_INT;
			case HE::Rendering::ShaderDataType::Bool:
				return GL_UNSIGNED_BYTE;
			case HE::Rendering::ShaderDataType::None:
				break;
		}

		HE_CORE_ASSERT(false, "Unknown shader data type");
		return GL_FLOAT;
	}

	uint32_t VertexAttribComponentCount(HE::Rendering::ShaderDataType dataType) {
		switch (dataType) {
			case HE::Rendering::ShaderDataType::Float:
			case HE::Rendering::ShaderDataType::Int:
			case HE::Rendering::ShaderDataType::Bool:
				return 1;
			case HE::Rendering::ShaderDataType::Float2:
			case HE::Rendering::ShaderDataType::Int2:
				return 2;
			case HE::Rendering::ShaderDataType::Float3:
			case HE::Rendering::ShaderDataType::Int3:
				return 3;
			case HE::Rendering::ShaderDataType::Float4:
			case HE::Rendering::ShaderDataType::Int4:
				return 4;
			case HE::Rendering::ShaderDataType::Mat3:
				return 3;
			case HE::Rendering::ShaderDataType::Mat4:
				return 4;
			case HE::Rendering::ShaderDataType::None:
				break;
		}

		HE_CORE_ASSERT(false, "Unknown shader data type");
		return 0;
	}

	bool IsIntegerVertexAttrib(HE::Rendering::ShaderDataType dataType) {
		switch (dataType) {
			case HE::Rendering::ShaderDataType::Int:
			case HE::Rendering::ShaderDataType::Int2:
			case HE::Rendering::ShaderDataType::Int3:
			case HE::Rendering::ShaderDataType::Int4:
			case HE::Rendering::ShaderDataType::Bool:
				return true;
			default:
				return false;
		}
	}

	bool ValidateTextureFile(const std::string& path) {
		std::error_code errorCode;
		if (!std::filesystem::exists(path, errorCode)) {
			HE_CORE_ERROR("Texture source file does not exist: {0}", path);
			return false;
		}

		int width = 0;
		int height = 0;
		int channels = 0;
		if (!stbi_info(path.c_str(), &width, &height, &channels)) {
			const char* reason = stbi_failure_reason();
			HE_CORE_ERROR("Failed to inspect texture source file: {0}", path);
			if (reason) {
				HE_CORE_ERROR("{0}", reason);
			}
			return false;
		}

		if (channels != 3 && channels != 4) {
			HE_CORE_ERROR("Unsupported texture channel count: {0}", channels);
			return false;
		}

		return true;
	}

	bool CompileTemporaryShader(GLenum shaderType, const std::string& sourceText, GLuint& shader) {
		shader = glCreateShader(shaderType);
		const GLchar* source = sourceText.c_str();
		glShaderSource(shader, 1, &source, nullptr);
		glCompileShader(shader);

		GLint isCompiled = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
		if (isCompiled == GL_TRUE) {
			return true;
		}

		GLint maxLength = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
		std::vector<GLchar> infoLog(maxLength > 0 ? maxLength : 1);
		glGetShaderInfoLog(shader, maxLength, &maxLength, infoLog.data());

		HE_CORE_ERROR("{0} shader compilation error", shaderType == GL_VERTEX_SHADER ? "Vertex" : "Fragment");
		HE_CORE_ERROR("{0}", infoLog.data());

		glDeleteShader(shader);
		shader = 0;
		return false;
	}

	bool ValidateShaderProgramSources(const HE::Rendering::ShaderProgramDesc& desc) {
		GLuint vertexShader = 0;
		GLuint fragmentShader = 0;
		if (!CompileTemporaryShader(GL_VERTEX_SHADER, desc.VertexSource, vertexShader)) {
			return false;
		}

		if (!CompileTemporaryShader(GL_FRAGMENT_SHADER, desc.FragmentSource, fragmentShader)) {
			glDeleteShader(vertexShader);
			return false;
		}

		GLuint program = glCreateProgram();
		glAttachShader(program, vertexShader);
		glAttachShader(program, fragmentShader);
		glLinkProgram(program);

		GLint isLinked = 0;
		glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
		if (isLinked == GL_FALSE) {
			GLint maxLength = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
			std::vector<GLchar> infoLog(maxLength > 0 ? maxLength : 1);
			glGetProgramInfoLog(program, maxLength, &maxLength, infoLog.data());

			HE_CORE_ERROR("Shader program link error");
			HE_CORE_ERROR("{0}", infoLog.data());

			glDeleteProgram(program);
			glDeleteShader(vertexShader);
			glDeleteShader(fragmentShader);
			return false;
		}

		glDetachShader(program, vertexShader);
		glDetachShader(program, fragmentShader);
		glDeleteProgram(program);
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);
		return true;
	}
}

namespace HE::Rendering {
	OpenGLGpuBuffer::OpenGLGpuBuffer(const GpuBufferDesc& desc, const void* initialData)
		: m_Desc(desc) {
		HE_CORE_ASSERT(m_Desc.Size > 0, "GPU buffer size must be greater than zero");

		glCreateBuffers(1, &m_RenderID);
		glNamedBufferData(m_RenderID, m_Desc.Size, initialData, GL_STATIC_DRAW);
	}

	OpenGLGpuBuffer::~OpenGLGpuBuffer() {
		glDeleteBuffers(1, &m_RenderID);
	}

	const GpuBufferDesc& OpenGLGpuBuffer::GetDesc() const {
		return m_Desc;
	}

	void OpenGLGpuBuffer::Bind() const {
		glBindBuffer(ToOpenGLBufferTarget(m_Desc.Usage), m_RenderID);
	}

	void OpenGLGpuBuffer::Unbind() const {
		glBindBuffer(ToOpenGLBufferTarget(m_Desc.Usage), 0);
	}

	uint32_t OpenGLGpuBuffer::GetRenderID() const {
		return m_RenderID;
	}

	OpenGLVertexBufferView::OpenGLVertexBufferView(const VertexBufferViewDesc& desc)
		: m_Desc(desc) {
		HE_CORE_ASSERT(m_Desc.VertexBuffer, "VertexBufferView requires a vertex buffer");
		HE_CORE_ASSERT(m_Desc.IndexBuffer, "VertexBufferView requires an index buffer");
		HE_CORE_ASSERT(m_Desc.VertexBuffer->GetDesc().Usage == GpuBufferUsage::Vertex, "VertexBufferView vertex buffer usage mismatch");
		HE_CORE_ASSERT(m_Desc.IndexBuffer->GetDesc().Usage == GpuBufferUsage::Index, "VertexBufferView index buffer usage mismatch");
		HE_CORE_ASSERT(m_Desc.IndexCount > 0, "VertexBufferView index count must be greater than zero");
		HE_CORE_ASSERT(!m_Desc.Layout.GetElements().empty(), "VertexBufferView requires vertex layout");

		GLint previousVertexArray = 0;
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVertexArray);
		GLint previousArrayBuffer = 0;
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer);

		glGenVertexArrays(1, &m_RenderID);
		glBindVertexArray(m_RenderID);
		m_Desc.VertexBuffer->Bind();

		uint32_t index = 0;
		const auto& layout = m_Desc.Layout;
		for (const auto& element : layout) {
			if (element.Type == ShaderDataType::Mat3 || element.Type == ShaderDataType::Mat4) {
				const uint32_t columnCount = element.Type == ShaderDataType::Mat3 ? 3 : 4;
				const uint32_t componentCount = VertexAttribComponentCount(element.Type);
				const uint32_t columnSize = static_cast<uint32_t>(sizeof(float)) * componentCount;

				for (uint32_t column = 0; column < columnCount; ++column) {
					glEnableVertexAttribArray(index);
					glVertexAttribPointer(
						index,
						componentCount,
						GL_FLOAT,
						element.Normalized ? GL_TRUE : GL_FALSE,
						layout.GetStride(),
						reinterpret_cast<const void*>(static_cast<std::uintptr_t>(element.Offset + columnSize * column)));
					++index;
				}

				continue;
			}

			glEnableVertexAttribArray(index);
			if (IsIntegerVertexAttrib(element.Type)) {
				glVertexAttribIPointer(
					index,
					VertexAttribComponentCount(element.Type),
					ToOpenGLType(element.Type),
					layout.GetStride(),
					reinterpret_cast<const void*>(static_cast<std::uintptr_t>(element.Offset)));
			}
			else {
				glVertexAttribPointer(
					index,
					VertexAttribComponentCount(element.Type),
					ToOpenGLType(element.Type),
					element.Normalized ? GL_TRUE : GL_FALSE,
					layout.GetStride(),
					reinterpret_cast<const void*>(static_cast<std::uintptr_t>(element.Offset)));
			}
			++index;
		}

		m_Desc.IndexBuffer->Bind();
		glBindBuffer(GL_ARRAY_BUFFER, previousArrayBuffer);
		glBindVertexArray(previousVertexArray);
	}

	OpenGLVertexBufferView::~OpenGLVertexBufferView() {
		glDeleteVertexArrays(1, &m_RenderID);
	}

	const VertexBufferViewDesc& OpenGLVertexBufferView::GetDesc() const {
		return m_Desc;
	}

	void OpenGLVertexBufferView::Bind() {
		glBindVertexArray(m_RenderID);
	}

	void OpenGLVertexBufferView::Unbind() {
		glBindVertexArray(0);
	}

	OpenGLRenderTarget::OpenGLRenderTarget(const RenderTargetDesc& desc)
		: m_Desc(desc), m_FrameBuffer(CreateRef<OpenGLFrameBuffer>(desc.Specification)) {}

	const RenderTargetDesc& OpenGLRenderTarget::GetDesc() const {
		return m_Desc;
	}

	void OpenGLRenderTarget::Bind() {
		m_FrameBuffer->Bind();
	}

	void OpenGLRenderTarget::Unbind() {
		m_FrameBuffer->Unbind();
	}

	void OpenGLRenderTarget::Resize(uint32_t width, uint32_t height) {
		m_FrameBuffer->Resize(width, height);
		m_Desc.Specification = m_FrameBuffer->GetSpecification();
	}

	void OpenGLRenderTarget::ClearAttachment(uint32_t index, int value) {
		m_FrameBuffer->ClearAttachment(index, value);
	}

	FrameBufferPixelRGBA8 OpenGLRenderTarget::ReadPixelRGBA8(uint32_t attachmentIndex, uint32_t x, uint32_t y) const {
		return m_FrameBuffer->ReadPixelRGBA8(attachmentIndex, x, y);
	}

	uint32_t OpenGLRenderTarget::GetRenderID() const {
		return m_FrameBuffer->GetRenderID();
	}

	uint32_t OpenGLRenderTarget::GetColorAttachment(uint32_t index) const {
		return m_FrameBuffer->GetColorAttachment(index);
	}

	const FrameBufferSpecification& OpenGLRenderTarget::GetSpecification() const {
		return m_FrameBuffer->GetSpecification();
	}

	OpenGLTextureResource::OpenGLTextureResource(const TextureDesc& desc)
		: m_Desc(desc), m_Texture(CreateRef<OpenGLTexture2D>(desc.SourcePath)) {}

	const TextureDesc& OpenGLTextureResource::GetDesc() const {
		return m_Desc;
	}

	uint32_t OpenGLTextureResource::GetRenderID() const {
		return m_Texture->GetRenderID();
	}

	uint32_t OpenGLTextureResource::GetWidth() const {
		return m_Texture->GetWidth();
	}

	uint32_t OpenGLTextureResource::GetHeight() const {
		return m_Texture->GetHeight();
	}

	void OpenGLTextureResource::Bind(uint32_t slot) {
		m_Texture->Bind(slot);
	}

	OpenGLShaderProgram::OpenGLShaderProgram(const ShaderProgramDesc& desc)
		: m_Desc(desc), m_Shader(CreateRef<OpenGLShader>(desc.VertexSource, desc.FragmentSource)) {}

	const ShaderProgramDesc& OpenGLShaderProgram::GetDesc() const {
		return m_Desc;
	}

	void OpenGLShaderProgram::Bind() {
		m_Shader->Bind();
	}

	void OpenGLShaderProgram::Unbind() {
		m_Shader->Unbind();
	}

	void OpenGLShaderProgram::SetInt(const std::string& name, int value) {
		m_Shader->SetInt(name, value);
	}

	void OpenGLShaderProgram::SetIntArray(const std::string& name, int* values, uint32_t size) {
		m_Shader->SetIntArray(name, values, size);
	}

	void OpenGLShaderProgram::SetFloat(const std::string& name, float value) {
		m_Shader->SetFloat(name, value);
	}

	void OpenGLShaderProgram::SetFloat2(const std::string& name, const glm::vec2 value) {
		m_Shader->SetFloat2(name, value);
	}

	void OpenGLShaderProgram::SetFloat3(const std::string& name, const glm::vec3 value) {
		m_Shader->SetFloat3(name, value);
	}

	void OpenGLShaderProgram::SetFloat4(const std::string& name, const glm::vec4 value) {
		m_Shader->SetFloat4(name, value);
	}

	void OpenGLShaderProgram::SetMat3(const std::string& name, const glm::mat3 value) {
		m_Shader->SetMat3(name, value);
	}

	void OpenGLShaderProgram::SetMat4(const std::string& name, const glm::mat4 value) {
		m_Shader->SetMat4(name, value);
	}

	void OpenGLCommandList::BeginRenderTarget(FrameBuffer& target) {
		m_CurrentLegacyTarget = &target;
		m_CurrentRenderTarget = nullptr;
		target.Bind();
	}

	void OpenGLCommandList::BeginRenderTarget(RenderTarget& target) {
		m_CurrentLegacyTarget = nullptr;
		m_CurrentRenderTarget = &target;
		target.Bind();
	}

	void OpenGLCommandList::ClearColor(const glm::vec4& color) {
		glClearColor(color.r, color.g, color.b, color.a);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	void OpenGLCommandList::BeginFrame(Camera& camera) {
		m_CurrentCamera = &camera;
	}

	void OpenGLCommandList::SetShaderProgram(ShaderProgram& shaderProgram) {
		m_CurrentShaderProgram = &shaderProgram;
		shaderProgram.Bind();
		if (m_HasFrameBinding) {
			m_CurrentShaderProgram->SetMat4("u_ViewProjection", m_CurrentFrameBinding.ViewProjection);
		}
		if (m_HasObjectBinding) {
			m_CurrentShaderProgram->SetMat4("u_Transform", m_CurrentObjectBinding.Transform);
		}
	}

	void OpenGLCommandList::SetVertexBufferView(VertexBufferView& vertexBufferView) {
		m_CurrentVertexBufferView = &vertexBufferView;
		vertexBufferView.Bind();
	}

	void OpenGLCommandList::SetFrameBinding(const FrameBinding& binding) {
		m_CurrentFrameBinding = binding;
		m_HasFrameBinding = true;
		if (m_CurrentShaderProgram) {
			m_CurrentShaderProgram->SetMat4("u_ViewProjection", binding.ViewProjection);
		}
	}

	void OpenGLCommandList::SetTexture(uint32_t slot, TextureResource& texture) {
		texture.Bind(slot);
	}

	void OpenGLCommandList::SetMat4(const std::string& name, const glm::mat4& value) {
		if (!m_CurrentShaderProgram) {
			HE_CORE_WARN("CommandList::SetMat4 skipped because no shader program is bound");
			return;
		}

		m_CurrentShaderProgram->SetMat4(name, value);
	}

	void OpenGLCommandList::SetMaterialBinding(const MaterialBinding& binding) {
		if (!m_CurrentShaderProgram) {
			HE_CORE_WARN("CommandList::SetMaterialBinding skipped because no shader program is bound");
			return;
		}

		for (const auto& parameter : binding.Parameters) {
			std::visit([&](auto&& value) {
				using T = std::decay_t<decltype(value)>;

				if constexpr (std::is_same_v<T, int>) {
					m_CurrentShaderProgram->SetInt(parameter.Name, value);
				}
				else if constexpr (std::is_same_v<T, float>) {
					m_CurrentShaderProgram->SetFloat(parameter.Name, value);
				}
				else if constexpr (std::is_same_v<T, glm::vec2>) {
					m_CurrentShaderProgram->SetFloat2(parameter.Name, value);
				}
				else if constexpr (std::is_same_v<T, glm::vec3>) {
					m_CurrentShaderProgram->SetFloat3(parameter.Name, value);
				}
				else if constexpr (std::is_same_v<T, glm::vec4>) {
					m_CurrentShaderProgram->SetFloat4(parameter.Name, value);
				}
				else if constexpr (std::is_same_v<T, glm::mat3>) {
					m_CurrentShaderProgram->SetMat3(parameter.Name, value);
				}
				else if constexpr (std::is_same_v<T, glm::mat4>) {
					m_CurrentShaderProgram->SetMat4(parameter.Name, value);
				}
				else if constexpr (std::is_same_v<T, std::vector<int>>) {
					m_CurrentShaderProgram->SetIntArray(parameter.Name, const_cast<int*>(value.data()), static_cast<uint32_t>(value.size()));
				}
				else if constexpr (std::is_same_v<T, std::vector<float>>) {
					HE_CORE_WARN("CommandList::SetMaterialBinding skipped unsupported float array parameter '{0}'", parameter.Name);
				}
				else if constexpr (std::is_same_v<T, Ref<Texture2D>>) {
					HE_CORE_WARN("CommandList::SetMaterialBinding received legacy texture parameter '{0}' in scalar parameter list", parameter.Name);
				}
			}, parameter.Value);
		}

		for (const auto& texture : binding.Textures) {
			if (!texture.Texture) {
				HE_CORE_WARN("CommandList::SetMaterialBinding skipped null texture parameter '{0}'", texture.Name);
				continue;
			}

			texture.Texture->Bind(texture.Slot);
			m_CurrentShaderProgram->SetInt(texture.Name, static_cast<int>(texture.Slot));
		}
	}

	void OpenGLCommandList::SetObjectBinding(const ObjectBinding& binding) {
		m_CurrentObjectBinding = binding;
		m_HasObjectBinding = true;
		if (m_CurrentShaderProgram) {
			m_CurrentShaderProgram->SetMat4("u_Transform", binding.Transform);
		}
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

	void OpenGLCommandList::DrawIndexed(uint32_t indexCount) {
		if (!m_CurrentShaderProgram) {
			HE_CORE_WARN("CommandList::DrawIndexed skipped because no shader program is bound");
			return;
		}

		if (!m_CurrentVertexBufferView) {
			HE_CORE_WARN("CommandList::DrawIndexed skipped because no vertex buffer view is bound");
			return;
		}

		if (!m_HasFrameBinding) {
			HE_CORE_WARN("CommandList::DrawIndexed skipped because no frame binding is active");
			return;
		}

		if (!m_HasObjectBinding) {
			HE_CORE_WARN("CommandList::DrawIndexed skipped because no object binding is active");
			return;
		}

		const uint32_t availableIndexCount = m_CurrentVertexBufferView->GetDesc().IndexCount;
		if (indexCount == 0) {
			HE_CORE_WARN("CommandList::DrawIndexed skipped because index count is zero");
			return;
		}

		if (indexCount > availableIndexCount) {
			HE_CORE_WARN("CommandList::DrawIndexed skipped because requested index count {0} exceeds bound vertex buffer view index count {1}", indexCount, availableIndexCount);
			return;
		}

		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT, nullptr);
	}

	void OpenGLCommandList::DrawIndexed(uint32_t indexCount, const glm::mat4& transform) {
		if (!m_CurrentCamera) {
			HE_CORE_WARN("CommandList::DrawIndexed compatibility helper skipped because no camera is active");
			return;
		}

		SetFrameBinding({ .ViewProjection = m_CurrentCamera->GetViewProjection() });
		SetObjectBinding({ .Transform = transform });
		DrawIndexed(indexCount);
	}

	void OpenGLCommandList::EndFrame() {
		m_CurrentCamera = nullptr;
		m_CurrentShaderProgram = nullptr;
		m_CurrentVertexBufferView = nullptr;
		m_CurrentFrameBinding = {};
		m_CurrentObjectBinding = {};
		m_HasFrameBinding = false;
		m_HasObjectBinding = false;
	}

	void OpenGLCommandList::EndRenderTarget() {
		if (m_CurrentRenderTarget) {
			m_CurrentRenderTarget->Unbind();
		}
		else if (m_CurrentLegacyTarget) {
			m_CurrentLegacyTarget->Unbind();
		}

		m_CurrentRenderTarget = nullptr;
		m_CurrentLegacyTarget = nullptr;
	}

	OpenGLRenderDevice::OpenGLRenderDevice() {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	CommandList& OpenGLRenderDevice::GetImmediateCommandList() {
		return m_ImmediateCommandList;
	}

	Ref<GpuBuffer> OpenGLRenderDevice::CreateBuffer(const GpuBufferDesc& desc, const void* initialData) {
		if (desc.Size == 0) {
			HE_CORE_ERROR("GPU buffer size must be greater than zero");
			return nullptr;
		}

		return CreateRef<OpenGLGpuBuffer>(desc, initialData);
	}

	Ref<VertexBufferView> OpenGLRenderDevice::CreateVertexBufferView(const VertexBufferViewDesc& desc) {
		if (!desc.VertexBuffer || !desc.IndexBuffer || desc.IndexCount == 0 || desc.Layout.GetElements().empty()) {
			HE_CORE_ERROR("Invalid vertex buffer view description");
			return nullptr;
		}

		if (desc.VertexBuffer->GetDesc().Usage != GpuBufferUsage::Vertex || desc.IndexBuffer->GetDesc().Usage != GpuBufferUsage::Index) {
			HE_CORE_ERROR("Vertex buffer view buffer usage mismatch");
			return nullptr;
		}

		return CreateRef<OpenGLVertexBufferView>(desc);
	}

	Ref<RenderTarget> OpenGLRenderDevice::CreateRenderTarget(const RenderTargetDesc& desc) {
		if (desc.Specification.Width == 0 || desc.Specification.Height == 0) {
			HE_CORE_ERROR("Render target dimensions must be greater than zero");
			return nullptr;
		}

		return CreateRef<OpenGLRenderTarget>(desc);
	}

	Ref<TextureResource> OpenGLRenderDevice::CreateTexture(const TextureDesc& desc) {
		if (desc.SourcePath.empty()) {
			HE_CORE_ERROR("Texture source path must not be empty");
			return nullptr;
		}

		if (!ValidateTextureFile(desc.SourcePath)) {
			return nullptr;
		}

		return CreateRef<OpenGLTextureResource>(desc);
	}

	Ref<ShaderProgram> OpenGLRenderDevice::CreateShaderProgram(const ShaderProgramDesc& desc) {
		if (desc.VertexSource.empty() || desc.FragmentSource.empty()) {
			HE_CORE_ERROR("Shader program sources must not be empty");
			return nullptr;
		}

		if (!ValidateShaderProgramSources(desc)) {
			return nullptr;
		}

		return CreateRef<OpenGLShaderProgram>(desc);
	}

	Ref<GpuBuffer> OpenGLRenderDevice::CreateVertexBuffer(float* vertices, uint32_t size) {
		return CreateBuffer({
			.Usage = GpuBufferUsage::Vertex,
			.Size = size,
			.Stride = 0
		}, vertices);
	}

	Ref<GpuBuffer> OpenGLRenderDevice::CreateIndexBuffer(uint32_t* indices, uint32_t count) {
		return CreateBuffer({
			.Usage = GpuBufferUsage::Index,
			.Size = count * static_cast<uint32_t>(sizeof(uint32_t)),
			.Stride = static_cast<uint32_t>(sizeof(uint32_t))
		}, indices);
	}
}
