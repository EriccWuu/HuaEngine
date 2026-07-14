#include "enginepch.h"
#include "OpenGLRenderDevice.h"

#include <filesystem>

#include "glad/glad.h"
#include "stb_image.h"

#include "HuaEngine/Rendering/Camera.h"
#include "HuaEngine/Rendering/RHI/RenderTargetTypes.h"
#include "HuaEngine/Rendering/Material/Material.h"
#include "HuaEngine/Rendering/RHI/RenderTarget.h"
#include "HuaEngine/Rendering/RHI/ShaderProgram.h"
#include "HuaEngine/Rendering/RHI/TextureResource.h"
#include "HuaEngine/Rendering/RHI/VertexBufferView.h"
#include "Platform/OpenGL/OpenGLRenderTargetStorage.h"
#include "Platform/OpenGL/OpenGLShader.h"

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

	GLenum ToOpenGLPrimitiveTopology(HE::Rendering::PrimitiveTopology topology) {
		switch (topology) {
			case HE::Rendering::PrimitiveTopology::TriangleList:
				return GL_TRIANGLES;
		}

		HE_CORE_ASSERT(false, "Unknown primitive topology");
		return GL_TRIANGLES;
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

	void OpenGLGpuBuffer::BindForCommandList() const {
		glBindBuffer(ToOpenGLBufferTarget(m_Desc.Usage), m_RenderID);
	}

	void OpenGLGpuBuffer::UnbindForCommandList() const {
		glBindBuffer(ToOpenGLBufferTarget(m_Desc.Usage), 0);
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
		static_cast<OpenGLGpuBuffer&>(*m_Desc.VertexBuffer).BindForCommandList();

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

		static_cast<OpenGLGpuBuffer&>(*m_Desc.IndexBuffer).BindForCommandList();
		glBindBuffer(GL_ARRAY_BUFFER, previousArrayBuffer);
		glBindVertexArray(previousVertexArray);
	}

	OpenGLVertexBufferView::~OpenGLVertexBufferView() {
		glDeleteVertexArrays(1, &m_RenderID);
	}

	const VertexBufferViewDesc& OpenGLVertexBufferView::GetDesc() const {
		return m_Desc;
	}

	void OpenGLVertexBufferView::BindForCommandList() {
		glBindVertexArray(m_RenderID);
	}

	void OpenGLVertexBufferView::UnbindForCommandList() {
		glBindVertexArray(0);
	}

	OpenGLBindGroupLayout::OpenGLBindGroupLayout(const BindGroupLayoutDesc& desc)
		: m_Desc(desc) {
		HE_CORE_ASSERT(!m_Desc.Entries.empty(), "BindGroupLayout requires entries");
	}

	const BindGroupLayoutDesc& OpenGLBindGroupLayout::GetDesc() const {
		return m_Desc;
	}

	OpenGLBindGroup::OpenGLBindGroup(const BindGroupDesc& desc)
		: m_Desc(desc) {
		HE_CORE_ASSERT(m_Desc.Layout, "BindGroup requires a layout");
		HE_CORE_ASSERT(!m_Desc.Entries.empty(), "BindGroup requires entries");
	}

	const BindGroupDesc& OpenGLBindGroup::GetDesc() const {
		return m_Desc;
	}

	OpenGLPipelineState::OpenGLPipelineState(const PipelineStateDesc& desc)
		: m_Desc(desc) {
		HE_CORE_ASSERT(m_Desc.Shader, "PipelineState requires a shader program");
		HE_CORE_ASSERT(!m_Desc.VertexLayout.GetElements().empty(), "PipelineState requires vertex input layout");
	}

	const PipelineStateDesc& OpenGLPipelineState::GetDesc() const {
		return m_Desc;
	}

	ShaderProgram& OpenGLPipelineState::GetShaderProgram() const {
		return *m_Desc.Shader;
	}

	PrimitiveTopology OpenGLPipelineState::GetTopology() const {
		return m_Desc.Topology;
	}

	OpenGLRenderTarget::OpenGLRenderTarget(const RenderTargetDesc& desc)
		: m_Desc(desc), m_BackendStorage(CreateRef<OpenGLRenderTargetStorage>(desc.Specification)) {}

	const RenderTargetDesc& OpenGLRenderTarget::GetDesc() const {
		return m_Desc;
	}

	void OpenGLRenderTarget::BeginForCommandList() {
		m_BackendStorage->BeginForCommandList();
	}

	void OpenGLRenderTarget::EndForCommandList() {
		m_BackendStorage->EndForCommandList();
	}

	void OpenGLRenderTarget::Resize(uint32_t width, uint32_t height) {
		m_BackendStorage->Resize(width, height);
		m_Desc.Specification = m_BackendStorage->GetSpecification();
	}

	void OpenGLRenderTarget::ClearAttachment(uint32_t index, int value) {
		m_BackendStorage->ClearAttachment(index, value);
	}

	RenderTargetPixelRGBA8 OpenGLRenderTarget::ReadPixelRGBA8(uint32_t attachmentIndex, uint32_t x, uint32_t y) const {
		return m_BackendStorage->ReadPixelRGBA8(attachmentIndex, x, y);
	}

	RenderTargetColorAttachmentView OpenGLRenderTarget::GetColorAttachmentView(uint32_t index) const {
		return { static_cast<uintptr_t>(m_BackendStorage->GetColorAttachment(index)) };
	}

	const RenderTargetSpecification& OpenGLRenderTarget::GetSpecification() const {
		return m_BackendStorage->GetSpecification();
	}

	OpenGLTextureResource::OpenGLTextureResource(const TextureDesc& desc)
		: m_Desc(desc) {
		stbi_set_flip_vertically_on_load(true);
		int width = 0;
		int height = 0;
		int channels = 0;
		stbi_uc* data = stbi_load(m_Desc.SourcePath.c_str(), &width, &height, &channels, 0);
		HE_CORE_ASSERT(data, "Failed to load image data");

		m_Width = static_cast<uint32_t>(width);
		m_Height = static_cast<uint32_t>(height);

		GLenum internalFormat = 0;
		GLenum format = 0;
		switch (channels) {
			case 3:
				internalFormat = GL_RGB8;
				format = GL_RGB;
				break;
			case 4:
				internalFormat = GL_RGBA8;
				format = GL_RGBA;
				break;
			default:
				break;
		}

		HE_CORE_ASSERT(internalFormat != 0 && format != 0, "Image format not supported");

		glCreateTextures(GL_TEXTURE_2D, 1, &m_RenderID);
		glTextureStorage2D(m_RenderID, 1, internalFormat, m_Width, m_Height);
		glTextureParameteri(m_RenderID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(m_RenderID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTextureSubImage2D(m_RenderID, 0, 0, 0, m_Width, m_Height, format, GL_UNSIGNED_BYTE, data);

		stbi_image_free(data);
	}

	OpenGLTextureResource::~OpenGLTextureResource() {
		glDeleteTextures(1, &m_RenderID);
	}

	const TextureDesc& OpenGLTextureResource::GetDesc() const {
		return m_Desc;
	}

	uint32_t OpenGLTextureResource::GetWidth() const {
		return m_Width;
	}

	uint32_t OpenGLTextureResource::GetHeight() const {
		return m_Height;
	}

	void OpenGLTextureResource::BindForCommandList(uint32_t slot) {
		glBindTextureUnit(slot, m_RenderID);
	}

	OpenGLShaderProgram::OpenGLShaderProgram(const ShaderProgramDesc& desc)
		: m_Desc(desc), m_Shader(CreateRef<OpenGLShader>(desc.VertexSource, desc.FragmentSource)) {}

	const ShaderProgramDesc& OpenGLShaderProgram::GetDesc() const {
		return m_Desc;
	}

	void OpenGLShaderProgram::BindForCommandList() {
		m_Shader->BindForCommandList();
	}

	void OpenGLShaderProgram::UnbindForCommandList() {
		m_Shader->UnbindForCommandList();
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

	void OpenGLCommandList::BeginRenderTarget(RenderTarget& target) {
		m_CurrentRenderTarget = &target;
		static_cast<OpenGLRenderTarget&>(target).BeginForCommandList();
	}

	void OpenGLCommandList::ClearColor(const glm::vec4& color) {
		glClearColor(color.r, color.g, color.b, color.a);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	void OpenGLCommandList::BeginFrame(Camera& camera) {
		m_CurrentCamera = &camera;
	}

	void OpenGLCommandList::SetPipelineState(PipelineState& pipelineState) {
		m_CurrentPipelineState = &pipelineState;
		auto& shaderProgram = static_cast<OpenGLPipelineState&>(pipelineState).GetShaderProgram();
		m_CurrentShaderProgram = &shaderProgram;
		m_HasFrameBindGroup = false;
		m_HasObjectBindGroup = false;
		static_cast<OpenGLShaderProgram&>(shaderProgram).BindForCommandList();
	}

	void OpenGLCommandList::SetVertexBufferView(VertexBufferView& vertexBufferView) {
		m_CurrentVertexBufferView = &vertexBufferView;
		static_cast<OpenGLVertexBufferView&>(vertexBufferView).BindForCommandList();
	}

	void OpenGLCommandList::SetBindGroup(uint32_t slot, BindGroup& bindGroup) {
		(void)slot;
		if (!m_CurrentShaderProgram) {
			HE_CORE_WARN("CommandList::SetBindGroup skipped because no shader program is bound");
			return;
		}

		auto& shaderProgram = static_cast<OpenGLShaderProgram&>(*m_CurrentShaderProgram);

		if (bindGroup.GetDesc().Layout) {
			const auto scope = bindGroup.GetDesc().Layout->GetDesc().Scope;
			if (scope == BindGroupScope::Frame) {
				m_HasFrameBindGroup = true;
			}
			else if (scope == BindGroupScope::Object) {
				m_HasObjectBindGroup = true;
			}
		}

		for (const auto& entry : bindGroup.GetDesc().Entries) {
			std::visit([&](auto&& value) {
				using T = std::decay_t<decltype(value)>;

				if constexpr (std::is_same_v<T, int>) {
					shaderProgram.SetInt(entry.Name, value);
				}
				else if constexpr (std::is_same_v<T, float>) {
					shaderProgram.SetFloat(entry.Name, value);
				}
				else if constexpr (std::is_same_v<T, glm::vec2>) {
					shaderProgram.SetFloat2(entry.Name, value);
				}
				else if constexpr (std::is_same_v<T, glm::vec3>) {
					shaderProgram.SetFloat3(entry.Name, value);
				}
				else if constexpr (std::is_same_v<T, glm::vec4>) {
					shaderProgram.SetFloat4(entry.Name, value);
				}
				else if constexpr (std::is_same_v<T, glm::mat3>) {
					shaderProgram.SetMat3(entry.Name, value);
				}
				else if constexpr (std::is_same_v<T, glm::mat4>) {
					shaderProgram.SetMat4(entry.Name, value);
				}
				else if constexpr (std::is_same_v<T, std::vector<int>>) {
					shaderProgram.SetIntArray(entry.Name, const_cast<int*>(value.data()), static_cast<uint32_t>(value.size()));
				}
				else if constexpr (std::is_same_v<T, Ref<TextureResource>>) {
					if (!value) {
						HE_CORE_WARN("CommandList::SetBindGroup skipped null texture binding '{0}'", entry.Name);
						return;
					}

					static_cast<OpenGLTextureResource&>(*value).BindForCommandList(entry.TextureSlot);
					shaderProgram.SetInt(entry.Name, static_cast<int>(entry.TextureSlot));
				}
			}, entry.Value);
		}
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

		if (!m_HasFrameBindGroup) {
			HE_CORE_WARN("CommandList::DrawIndexed skipped because no frame bind group is active");
			return;
		}

		if (!m_HasObjectBindGroup) {
			HE_CORE_WARN("CommandList::DrawIndexed skipped because no object bind group is active");
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

		GLenum topology = GL_TRIANGLES;
		if (m_CurrentPipelineState) {
			topology = ToOpenGLPrimitiveTopology(static_cast<OpenGLPipelineState&>(*m_CurrentPipelineState).GetTopology());
		}

		glDrawElements(topology, static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT, nullptr);
	}

	void OpenGLCommandList::EndFrame() {
		m_CurrentCamera = nullptr;
		m_CurrentShaderProgram = nullptr;
		m_CurrentPipelineState = nullptr;
		m_CurrentVertexBufferView = nullptr;
		m_HasFrameBindGroup = false;
		m_HasObjectBindGroup = false;
	}

	void OpenGLCommandList::EndRenderTarget() {
		if (m_CurrentRenderTarget) {
			static_cast<OpenGLRenderTarget*>(m_CurrentRenderTarget)->EndForCommandList();
		}

		m_CurrentRenderTarget = nullptr;
	}

	OpenGLRenderDevice::OpenGLRenderDevice()
		: OpenGLRenderDevice(RenderDeviceDesc{}) {}

	OpenGLRenderDevice::OpenGLRenderDevice(const RenderDeviceDesc& desc)
		: m_Desc(desc) {
		m_Capabilities.Backend = RenderBackendType::OpenGL;
		m_Capabilities.BackendName = "OpenGL";
		m_Capabilities.SupportsPipelineState = true;
		m_Capabilities.SupportsBindGroups = true;
		m_Capabilities.SupportsRenderGraphResources = true;
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	const RenderDeviceDesc& OpenGLRenderDevice::GetDesc() const {
		return m_Desc;
	}

	const RenderDeviceCapabilities& OpenGLRenderDevice::GetCapabilities() const {
		return m_Capabilities;
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

	Ref<PipelineState> OpenGLRenderDevice::CreatePipelineState(const PipelineStateDesc& desc) {
		if (!desc.Shader || desc.VertexLayout.GetElements().empty()) {
			HE_CORE_ERROR("Invalid pipeline state description");
			return nullptr;
		}

		return CreateRef<OpenGLPipelineState>(desc);
	}

	Ref<BindGroupLayout> OpenGLRenderDevice::CreateBindGroupLayout(const BindGroupLayoutDesc& desc) {
		if (desc.Entries.empty()) {
			HE_CORE_ERROR("Bind group layout must have at least one entry");
			return nullptr;
		}

		return CreateRef<OpenGLBindGroupLayout>(desc);
	}

	Ref<BindGroup> OpenGLRenderDevice::CreateBindGroup(const BindGroupDesc& desc) {
		if (!desc.Layout || desc.Entries.empty()) {
			HE_CORE_ERROR("Invalid bind group description");
			return nullptr;
		}

		return CreateRef<OpenGLBindGroup>(desc);
	}

}
