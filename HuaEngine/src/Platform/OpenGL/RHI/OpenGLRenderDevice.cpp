#include "enginepch.h"
#include "OpenGLRenderDevice.h"

#include <algorithm>
#include <filesystem>

#include "glad/glad.h"
#include "stb_image.h"

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

	bool BindGroupLayoutEntriesMatch(const HE::Rendering::BindGroupLayoutDesc& expected, const HE::Rendering::BindGroupLayoutDesc& actual) {
		if (expected.Scope != actual.Scope || expected.Entries.size() != actual.Entries.size()) {
			return false;
		}

		for (size_t i = 0; i < expected.Entries.size(); ++i) {
			const auto& expectedEntry = expected.Entries[i];
			const auto& actualEntry = actual.Entries[i];
			if (expectedEntry.Name != actualEntry.Name || expectedEntry.Type != actualEntry.Type || expectedEntry.Binding != actualEntry.Binding) {
				return false;
			}
		}

		return true;
	}

	bool ValidatePipelineBindGroupLayouts(const HE::Rendering::PipelineStateDesc& desc) {
		std::vector<uint32_t> slots;
		slots.reserve(desc.BindGroupLayouts.size());

		for (const auto& layoutRef : desc.BindGroupLayouts) {
			if (!layoutRef.Layout) {
				HE_CORE_ERROR("Pipeline state bind group layout slot {0} is null", layoutRef.Slot);
				return false;
			}

			if (std::find(slots.begin(), slots.end(), layoutRef.Slot) != slots.end()) {
				HE_CORE_ERROR("Pipeline state bind group layout slot {0} is duplicated", layoutRef.Slot);
				return false;
			}

			slots.push_back(layoutRef.Slot);
		}

		return true;
	}

	GLenum ToOpenGLTextureInternalFormat(HE::Rendering::RenderTargetTextureFormat format) {
		switch (format) {
			case HE::Rendering::RenderTargetTextureFormat::RGBA8:
				return GL_RGBA8;
			case HE::Rendering::RenderTargetTextureFormat::RED_INTEGER:
				return GL_R32I;
			case HE::Rendering::RenderTargetTextureFormat::DEPTH24_STENCIL8:
				return GL_DEPTH24_STENCIL8;
			case HE::Rendering::RenderTargetTextureFormat::None:
				return 0;
		}

		return 0;
	}

	GLint ToOpenGLSamplerFilter(HE::Rendering::SamplerFilter filter) {
		switch (filter) {
			case HE::Rendering::SamplerFilter::Nearest:
				return GL_NEAREST;
			case HE::Rendering::SamplerFilter::Linear:
				return GL_LINEAR;
		}

		return GL_LINEAR;
	}

	GLint ToOpenGLSamplerAddressMode(HE::Rendering::SamplerAddressMode mode) {
		switch (mode) {
			case HE::Rendering::SamplerAddressMode::Repeat:
				return GL_REPEAT;
			case HE::Rendering::SamplerAddressMode::ClampToEdge:
				return GL_CLAMP_TO_EDGE;
		}

		return GL_REPEAT;
	}

	GLenum ToOpenGLBlendFactor(HE::Rendering::BlendFactor factor) {
		switch (factor) {
			case HE::Rendering::BlendFactor::Zero:
				return GL_ZERO;
			case HE::Rendering::BlendFactor::One:
				return GL_ONE;
			case HE::Rendering::BlendFactor::SrcAlpha:
				return GL_SRC_ALPHA;
			case HE::Rendering::BlendFactor::OneMinusSrcAlpha:
				return GL_ONE_MINUS_SRC_ALPHA;
		}

		return GL_ONE;
	}

	GLenum ToOpenGLBlendOp(HE::Rendering::BlendOp op) {
		switch (op) {
			case HE::Rendering::BlendOp::Add:
				return GL_FUNC_ADD;
		}

		return GL_FUNC_ADD;
	}

	GLenum ToOpenGLCompareOp(HE::Rendering::CompareOp op) {
		switch (op) {
			case HE::Rendering::CompareOp::Never:
				return GL_NEVER;
			case HE::Rendering::CompareOp::Less:
				return GL_LESS;
			case HE::Rendering::CompareOp::Equal:
				return GL_EQUAL;
			case HE::Rendering::CompareOp::LessEqual:
				return GL_LEQUAL;
			case HE::Rendering::CompareOp::Greater:
				return GL_GREATER;
			case HE::Rendering::CompareOp::NotEqual:
				return GL_NOTEQUAL;
			case HE::Rendering::CompareOp::GreaterEqual:
				return GL_GEQUAL;
			case HE::Rendering::CompareOp::Always:
				return GL_ALWAYS;
		}

		return GL_LEQUAL;
	}

	GLenum ToOpenGLFrontFace(HE::Rendering::FrontFace frontFace) {
		switch (frontFace) {
			case HE::Rendering::FrontFace::CounterClockwise:
				return GL_CCW;
			case HE::Rendering::FrontFace::Clockwise:
				return GL_CW;
		}

		return GL_CCW;
	}

	GLenum ToOpenGLPolygonMode(HE::Rendering::FillMode fillMode) {
		switch (fillMode) {
			case HE::Rendering::FillMode::Solid:
				return GL_FILL;
			case HE::Rendering::FillMode::Wireframe:
				return GL_LINE;
		}

		return GL_FILL;
	}

	bool ValidateTextureDesc(const HE::Rendering::TextureDesc& desc) {
		if (!desc.SourcePath.empty()) {
			return ValidateTextureFile(desc.SourcePath);
		}

		if (desc.Width == 0 || desc.Height == 0) {
			HE_CORE_ERROR("Texture dimensions must be greater than zero");
			return false;
		}

		if (desc.Format == HE::Rendering::RenderTargetTextureFormat::None || ToOpenGLTextureInternalFormat(desc.Format) == 0) {
			HE_CORE_ERROR("Texture format must be concrete and supported");
			return false;
		}

		if (desc.Usage == HE::Rendering::TextureUsageNone) {
			HE_CORE_ERROR("Texture usage flags must not be empty");
			return false;
		}

		if (desc.MipLevels == 0 || desc.Samples == 0) {
			HE_CORE_ERROR("Texture mip levels and samples must be greater than zero");
			return false;
		}

		return true;
	}

	bool IsColorTargetFormat(HE::Rendering::RenderTargetTextureFormat format) {
		switch (format) {
			case HE::Rendering::RenderTargetTextureFormat::RGBA8:
			case HE::Rendering::RenderTargetTextureFormat::RED_INTEGER:
				return true;
			case HE::Rendering::RenderTargetTextureFormat::None:
			case HE::Rendering::RenderTargetTextureFormat::DEPTH24_STENCIL8:
				return false;
		}

		return false;
	}

	bool IsDepthStencilFormat(HE::Rendering::RenderTargetTextureFormat format) {
		switch (format) {
			case HE::Rendering::RenderTargetTextureFormat::None:
			case HE::Rendering::RenderTargetTextureFormat::DEPTH24_STENCIL8:
				return true;
			case HE::Rendering::RenderTargetTextureFormat::RGBA8:
			case HE::Rendering::RenderTargetTextureFormat::RED_INTEGER:
				return false;
		}

		return false;
	}

	bool ValidatePipelineRenderState(const HE::Rendering::PipelineStateDesc& desc) {
		if (desc.ColorTargets.empty()) {
			HE_CORE_ERROR("Pipeline state must declare at least one color target");
			return false;
		}

		for (const auto& colorTarget : desc.ColorTargets) {
			if (!IsColorTargetFormat(colorTarget.Format)) {
				HE_CORE_ERROR("Pipeline state color target format is invalid");
				return false;
			}
		}

		if (!IsDepthStencilFormat(desc.DepthStencil.Format)) {
			HE_CORE_ERROR("Pipeline state depth/stencil format is invalid");
			return false;
		}

		return true;
	}

	bool PipelineMatchesCurrentAttachmentFormats(
		const HE::Rendering::PipelineStateDesc& pipelineDesc,
		HE::Rendering::RenderTargetTextureFormat colorFormat,
		HE::Rendering::RenderTargetTextureFormat depthStencilFormat) {
		if (pipelineDesc.ColorTargets.size() != 1) {
			HE_CORE_WARN("CommandList::DrawIndexed skipped because OpenGL backend currently supports exactly one pipeline color target");
			return false;
		}

		const auto pipelineColorFormat = pipelineDesc.ColorTargets[0].Format;
		if (colorFormat != pipelineColorFormat) {
			HE_CORE_WARN("CommandList::DrawIndexed skipped because pipeline color target format does not match current render target");
			return false;
		}

		const auto pipelineDepthStencilFormat = pipelineDesc.DepthStencil.Format;
		if (pipelineDepthStencilFormat != HE::Rendering::RenderTargetTextureFormat::None
			&& depthStencilFormat != pipelineDepthStencilFormat) {
			HE_CORE_WARN("CommandList::DrawIndexed skipped because pipeline depth/stencil format does not match current render target");
			return false;
		}

		return true;
	}
}

namespace HE::Rendering {
	OpenGLCommandList::~OpenGLCommandList() {
		ReleaseExplicitVertexArray();
	}

	void OpenGLCommandList::ReleaseExplicitVertexArray() {
		if (m_ExplicitVertexArray != 0) {
			glDeleteVertexArrays(1, &m_ExplicitVertexArray);
			m_ExplicitVertexArray = 0;
		}
	}

	void OpenGLCommandList::RebuildExplicitVertexArray() {
		ReleaseExplicitVertexArray();

		if (!m_CurrentPipelineState || !m_HasExplicitVertexBuffer || !m_HasExplicitIndexBuffer) {
			return;
		}

		if (!m_CurrentVertexBufferBinding.Buffer || !m_CurrentIndexBufferBinding.Buffer) {
			return;
		}

		if (m_CurrentVertexBufferBinding.Buffer->GetDesc().Usage != GpuBufferUsage::Vertex
			|| m_CurrentIndexBufferBinding.Buffer->GetDesc().Usage != GpuBufferUsage::Index) {
			HE_CORE_WARN("CommandList explicit vertex/index binding skipped because buffer usage does not match");
			return;
		}

		const auto& layout = m_CurrentPipelineState->GetDesc().VertexLayout;
		if (layout.GetElements().empty()) {
			return;
		}

		GLint previousVertexArray = 0;
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVertexArray);
		GLint previousArrayBuffer = 0;
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer);

		glGenVertexArrays(1, &m_ExplicitVertexArray);
		glBindVertexArray(m_ExplicitVertexArray);
		static_cast<OpenGLGpuBuffer&>(*m_CurrentVertexBufferBinding.Buffer).BindForCommandList();

		uint32_t index = 0;
		const auto stride = m_CurrentVertexBufferBinding.Stride != 0 ? m_CurrentVertexBufferBinding.Stride : layout.GetStride();
		for (const auto& element : layout) {
			const auto offset = static_cast<std::uintptr_t>(m_CurrentVertexBufferBinding.Offset + element.Offset);
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
						stride,
						reinterpret_cast<const void*>(offset + columnSize * column));
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
					stride,
					reinterpret_cast<const void*>(offset));
			}
			else {
				glVertexAttribPointer(
					index,
					VertexAttribComponentCount(element.Type),
					ToOpenGLType(element.Type),
					element.Normalized ? GL_TRUE : GL_FALSE,
					stride,
					reinterpret_cast<const void*>(offset));
			}
			++index;
		}

		static_cast<OpenGLGpuBuffer&>(*m_CurrentIndexBufferBinding.Buffer).BindForCommandList();
		glBindBuffer(GL_ARRAY_BUFFER, previousArrayBuffer);
		glBindVertexArray(previousVertexArray);
	}

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
		HE_CORE_ASSERT(ValidatePipelineBindGroupLayouts(m_Desc), "PipelineState bind group layout contract is invalid");
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
		: m_Desc(desc), m_BackendStorage(CreateRef<OpenGLRenderTargetStorage>(desc.Specification)) {
		for (const auto& attachment : m_Desc.Specification.Attachments.Attachments) {
			const bool isDepthStencil = attachment.Format == RenderTargetTextureFormat::DEPTH24_STENCIL8;
			TextureDesc textureDesc{
				.Width = m_Desc.Specification.Width,
				.Height = m_Desc.Specification.Height,
				.Format = attachment.Format,
				.Usage = isDepthStencil
					? TextureUsageDepthStencilAttachment | TextureUsageSampled
					: TextureUsageColorAttachment | TextureUsageSampled,
				.MipLevels = 1,
				.Samples = m_Desc.Specification.Samples
			};
			auto texture = CreateRef<OpenGLTextureResource>(textureDesc, m_BackendStorage, static_cast<uint32_t>(m_ColorAttachmentTextures.size()), isDepthStencil);
			auto textureView = CreateRef<OpenGLTextureView>(TextureViewDesc{ .Texture = texture, .Format = attachment.Format });
			if (isDepthStencil) {
				m_DepthStencilAttachmentTexture = texture;
				m_DepthStencilAttachmentTextureView = textureView;
			} else {
				m_ColorAttachmentTextures.push_back(texture);
				m_ColorAttachmentTextureViews.push_back(textureView);
			}
		}
	}

	const RenderTargetDesc& OpenGLRenderTarget::GetDesc() const {
		return m_Desc;
	}

	void OpenGLRenderTarget::BeginForCommandList() {
		m_BackendStorage->BeginForCommandList();
	}

	void OpenGLRenderTarget::EndForCommandList() {
		m_BackendStorage->EndForCommandList();
	}

	OpenGLRenderTargetStorage* OpenGLRenderTarget::GetAttachmentStorage() const {
		return m_BackendStorage.get();
	}

	void OpenGLRenderTarget::Resize(uint32_t width, uint32_t height) {
		m_BackendStorage->Resize(width, height);
		m_Desc.Specification = m_BackendStorage->GetSpecification();
		for (uint32_t index = 0; index < m_ColorAttachmentTextures.size(); ++index) {
			auto texture = std::static_pointer_cast<OpenGLTextureResource>(m_ColorAttachmentTextures[index]);
			auto desc = texture->GetDesc();
			desc.Width = width;
			desc.Height = height;
			texture->UpdateAttachmentDesc(desc);
		}
		if (m_DepthStencilAttachmentTexture) {
			auto texture = std::static_pointer_cast<OpenGLTextureResource>(m_DepthStencilAttachmentTexture);
			auto desc = texture->GetDesc();
			desc.Width = width;
			desc.Height = height;
			texture->UpdateAttachmentDesc(desc);
		}
	}

	void OpenGLRenderTarget::ClearAttachment(uint32_t index, int value) {
		m_BackendStorage->ClearAttachment(index, value);
	}

	RenderTargetPixelRGBA8 OpenGLRenderTarget::ReadPixelRGBA8(uint32_t attachmentIndex, uint32_t x, uint32_t y) const {
		return m_BackendStorage->ReadPixelRGBA8(attachmentIndex, x, y);
	}

	Ref<TextureResource> OpenGLRenderTarget::GetColorAttachmentTexture(uint32_t index) const {
		HE_CORE_ASSERT(index < m_ColorAttachmentTextures.size(), "Color attachment index out of range");
		return m_ColorAttachmentTextures[index];
	}

	Ref<TextureResource> OpenGLRenderTarget::GetDepthStencilAttachmentTexture() const {
		return m_DepthStencilAttachmentTexture;
	}

	Ref<TextureView> OpenGLRenderTarget::GetColorAttachmentTextureView(uint32_t index) const {
		HE_CORE_ASSERT(index < m_ColorAttachmentTextureViews.size(), "Color attachment index out of range");
		return m_ColorAttachmentTextureViews[index];
	}

	Ref<TextureView> OpenGLRenderTarget::GetDepthStencilAttachmentTextureView() const {
		return m_DepthStencilAttachmentTextureView;
	}

	RenderTargetColorAttachmentView OpenGLRenderTarget::GetColorAttachmentView(uint32_t index) const {
		const auto& specification = m_BackendStorage->GetSpecification();
		HE_CORE_ASSERT(index < specification.Attachments.Attachments.size(), "Color attachment index out of range");
		return {
			.NativeHandle = static_cast<uintptr_t>(m_BackendStorage->GetColorAttachment(index)),
			.Format = specification.Attachments.Attachments[index].Format,
			.Width = specification.Width,
			.Height = specification.Height,
			.Samples = specification.Samples,
			.AttachmentIndex = index
		};
	}

	RenderTargetColorAttachmentView OpenGLRenderTarget::GetDepthStencilAttachmentView() const {
		const auto& specification = m_BackendStorage->GetSpecification();
		RenderTargetTextureFormat format = RenderTargetTextureFormat::None;
		for (const auto& attachment : specification.Attachments.Attachments) {
			if (attachment.Format == RenderTargetTextureFormat::DEPTH24_STENCIL8) {
				format = attachment.Format;
				break;
			}
		}

		return {
			.NativeHandle = static_cast<uintptr_t>(m_BackendStorage->GetDepthAttachment()),
			.Format = format,
			.Width = specification.Width,
			.Height = specification.Height,
			.Samples = specification.Samples,
			.AttachmentIndex = 0
		};
	}

	const RenderTargetSpecification& OpenGLRenderTarget::GetSpecification() const {
		return m_BackendStorage->GetSpecification();
	}

	OpenGLTextureResource::OpenGLTextureResource(const TextureDesc& desc)
		: m_Desc(desc) {
		if (!m_Desc.SourcePath.empty()) {
			stbi_set_flip_vertically_on_load(true);
			int width = 0;
			int height = 0;
			int channels = 0;
			stbi_uc* data = stbi_load(m_Desc.SourcePath.c_str(), &width, &height, &channels, 4);
			HE_CORE_ASSERT(data, "Failed to load image data");

			m_Width = static_cast<uint32_t>(width);
			m_Height = static_cast<uint32_t>(height);
			m_Desc.Width = m_Width;
			m_Desc.Height = m_Height;
			m_Desc.Format = RenderTargetTextureFormat::RGBA8;
			m_Desc.Usage = TextureUsageSampled;
			m_Desc.MipLevels = 1;
			m_Desc.Samples = 1;

			glCreateTextures(GL_TEXTURE_2D, 1, &m_RenderID);
			glTextureStorage2D(m_RenderID, 1, GL_RGBA8, m_Width, m_Height);
			glTextureParameteri(m_RenderID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTextureParameteri(m_RenderID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTextureSubImage2D(m_RenderID, 0, 0, 0, m_Width, m_Height, GL_RGBA, GL_UNSIGNED_BYTE, data);

			stbi_image_free(data);
			return;
		}

		m_Width = m_Desc.Width;
		m_Height = m_Desc.Height;

		glCreateTextures(GL_TEXTURE_2D, 1, &m_RenderID);
		glTextureStorage2D(
			m_RenderID,
			static_cast<GLsizei>(m_Desc.MipLevels),
			ToOpenGLTextureInternalFormat(m_Desc.Format),
			static_cast<GLsizei>(m_Width),
			static_cast<GLsizei>(m_Height));
		glTextureParameteri(m_RenderID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(m_RenderID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	OpenGLTextureResource::OpenGLTextureResource(TextureDesc desc, Ref<OpenGLRenderTargetStorage> attachmentStorage, uint32_t attachmentIndex, bool isDepthStencil)
		: m_Desc(std::move(desc)),
		  m_Width(m_Desc.Width),
		  m_Height(m_Desc.Height),
		  m_AttachmentStorage(std::move(attachmentStorage)),
		  m_AttachmentIndex(attachmentIndex),
		  m_IsDepthStencilAttachment(isDepthStencil) {
		HE_CORE_ASSERT(m_AttachmentStorage, "Attachment texture storage must not be null");
	}

	OpenGLTextureResource::~OpenGLTextureResource() {
		if (!m_AttachmentStorage && m_RenderID != 0) {
			glDeleteTextures(1, &m_RenderID);
		}
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
		const uint32_t renderID = m_AttachmentStorage
			? (m_IsDepthStencilAttachment ? m_AttachmentStorage->GetDepthAttachment() : m_AttachmentStorage->GetColorAttachment(m_AttachmentIndex))
			: m_RenderID;
		glBindTextureUnit(slot, renderID);
	}

	void OpenGLTextureResource::UpdateAttachmentDesc(const TextureDesc& desc) {
		HE_CORE_ASSERT(m_AttachmentStorage, "Only attachment textures can update from render target resize");
		m_Desc = desc;
		m_Width = desc.Width;
		m_Height = desc.Height;
	}

	OpenGLRenderTargetStorage* OpenGLTextureResource::GetAttachmentStorage() const {
		return m_AttachmentStorage.get();
	}

	OpenGLTextureView::OpenGLTextureView(const TextureViewDesc& desc)
		: m_Desc(desc) {}

	const TextureViewDesc& OpenGLTextureView::GetDesc() const {
		return m_Desc;
	}

	void OpenGLTextureView::BindForCommandList(uint32_t slot) {
		if (!m_Desc.Texture) {
			return;
		}

		static_cast<OpenGLTextureResource&>(*m_Desc.Texture).BindForCommandList(slot);
	}

	OpenGLSampler::OpenGLSampler(const SamplerDesc& desc)
		: m_Desc(desc) {
		glCreateSamplers(1, &m_RenderID);
		glSamplerParameteri(m_RenderID, GL_TEXTURE_MIN_FILTER, ToOpenGLSamplerFilter(m_Desc.MinFilter));
		glSamplerParameteri(m_RenderID, GL_TEXTURE_MAG_FILTER, ToOpenGLSamplerFilter(m_Desc.MagFilter));
		glSamplerParameteri(m_RenderID, GL_TEXTURE_WRAP_S, ToOpenGLSamplerAddressMode(m_Desc.AddressU));
		glSamplerParameteri(m_RenderID, GL_TEXTURE_WRAP_T, ToOpenGLSamplerAddressMode(m_Desc.AddressV));
		glSamplerParameteri(m_RenderID, GL_TEXTURE_WRAP_R, ToOpenGLSamplerAddressMode(m_Desc.AddressW));
	}

	OpenGLSampler::~OpenGLSampler() {
		glDeleteSamplers(1, &m_RenderID);
	}

	const SamplerDesc& OpenGLSampler::GetDesc() const {
		return m_Desc;
	}

	void OpenGLSampler::BindForCommandList(uint32_t slot) {
		glBindSampler(slot, m_RenderID);
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

	void OpenGLCommandList::BeginRenderPass(const RenderPassDesc& desc) {
		if (m_CurrentRenderTargetStorage) {
			HE_CORE_WARN("OpenGLCommandList::BeginRenderPass skipped because a render pass is already active");
			return;
		}

		if (desc.ColorAttachments.empty()) {
			HE_CORE_WARN("OpenGLCommandList::BeginRenderPass skipped because no color attachment was provided");
			return;
		}

		if (desc.ColorAttachments.size() > 1) {
			HE_CORE_WARN("OpenGLCommandList::BeginRenderPass skipped because multiple color attachments are not supported yet");
			return;
		}

		const auto& colorAttachment = desc.ColorAttachments[0];
		const auto colorView = colorAttachment.View
			? colorAttachment.View
			: (colorAttachment.Target ? colorAttachment.Target->GetColorAttachmentTextureView(colorAttachment.AttachmentIndex) : nullptr);
		if (!colorView || !colorView->GetDesc().Texture) {
			HE_CORE_WARN("OpenGLCommandList::BeginRenderPass skipped because the first color attachment view is null");
			return;
		}

		auto& colorTexture = static_cast<OpenGLTextureResource&>(*colorView->GetDesc().Texture);
		auto* colorStorage = colorTexture.GetAttachmentStorage();
		if (!colorStorage) {
			HE_CORE_WARN("OpenGLCommandList::BeginRenderPass skipped because the color attachment view is not render-target backed");
			return;
		}

		Ref<TextureView> depthStencilView;
		if (desc.DepthStencilAttachment) {
			const auto& depthAttachment = *desc.DepthStencilAttachment;
			depthStencilView = depthAttachment.View
				? depthAttachment.View
				: (depthAttachment.Target ? depthAttachment.Target->GetDepthStencilAttachmentTextureView() : nullptr);
			if (!depthStencilView || !depthStencilView->GetDesc().Texture) {
				HE_CORE_WARN("OpenGLCommandList::BeginRenderPass skipped because the depth/stencil attachment view is null");
				return;
			}

			auto& depthTexture = static_cast<OpenGLTextureResource&>(*depthStencilView->GetDesc().Texture);
			if (depthTexture.GetAttachmentStorage() != colorStorage) {
				HE_CORE_WARN("OpenGLCommandList::BeginRenderPass skipped because independent depth/stencil targets are not supported yet");
				return;
			}
		}

		m_CurrentRenderTarget = colorAttachment.Target.get();
		m_CurrentRenderTargetStorage = colorStorage;
		m_CurrentColorAttachmentFormat = colorView->GetDesc().Format;
		m_CurrentDepthStencilAttachmentFormat = depthStencilView ? depthStencilView->GetDesc().Format : RenderTargetTextureFormat::None;
		m_CurrentRenderTargetStorage->BeginForCommandList();

		GLbitfield clearMask = 0;
		if (colorAttachment.Load == LoadOp::Clear) {
			const auto& clear = colorAttachment.ClearColor;
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glClearColor(clear.r, clear.g, clear.b, clear.a);
			clearMask |= GL_COLOR_BUFFER_BIT;
		}

		if (desc.DepthStencilAttachment && desc.DepthStencilAttachment->DepthLoad == LoadOp::Clear) {
			glDepthMask(GL_TRUE);
			glClearDepth(desc.DepthStencilAttachment->ClearDepth);
			clearMask |= GL_DEPTH_BUFFER_BIT;
		}

		if (clearMask != 0) {
			glClear(clearMask);
		}
	}

	void OpenGLCommandList::EndRenderPass() {
		if (m_CurrentRenderTargetStorage) {
			m_CurrentRenderTargetStorage->EndForCommandList();
		}

		m_CurrentRenderTarget = nullptr;
		m_CurrentRenderTargetStorage = nullptr;
		m_CurrentColorAttachmentFormat = RenderTargetTextureFormat::None;
		m_CurrentDepthStencilAttachmentFormat = RenderTargetTextureFormat::None;
	}

	void OpenGLCommandList::ResourceBarrier(const HE::Rendering::ResourceBarrier& barrier) {
		if (!barrier.Texture) {
			HE_CORE_WARN("OpenGLCommandList::ResourceBarrier skipped null texture barrier");
			return;
		}
	}

	void OpenGLCommandList::BeginRenderTarget(RenderTarget& target) {
		m_CurrentRenderTarget = &target;
		m_CurrentRenderTargetStorage = static_cast<OpenGLRenderTarget&>(target).GetAttachmentStorage();
		m_CurrentColorAttachmentFormat = target.GetColorAttachmentTextureView()->GetDesc().Format;
		const auto depthStencilView = target.GetDepthStencilAttachmentTextureView();
		m_CurrentDepthStencilAttachmentFormat = depthStencilView ? depthStencilView->GetDesc().Format : RenderTargetTextureFormat::None;
		static_cast<OpenGLRenderTarget&>(target).BeginForCommandList();
	}

	void OpenGLCommandList::ClearColor(const glm::vec4& color) {
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glClearColor(color.r, color.g, color.b, color.a);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	void OpenGLCommandList::BeginFrame() {
	}

	void OpenGLCommandList::SetPipelineState(PipelineState& pipelineState) {
		m_CurrentPipelineState = &pipelineState;
		auto& openGLPipelineState = static_cast<OpenGLPipelineState&>(pipelineState);
		const auto& pipelineDesc = openGLPipelineState.GetDesc();
		auto& shaderProgram = openGLPipelineState.GetShaderProgram();
		m_CurrentShaderProgram = &shaderProgram;
		m_BoundBindGroupSlots.clear();

		if (pipelineDesc.Raster.Cull == CullMode::None) {
			glDisable(GL_CULL_FACE);
		}
		else {
			glEnable(GL_CULL_FACE);
			glCullFace(pipelineDesc.Raster.Cull == CullMode::Front ? GL_FRONT : GL_BACK);
		}
		glFrontFace(ToOpenGLFrontFace(pipelineDesc.Raster.FrontFaceMode));
		glPolygonMode(GL_FRONT_AND_BACK, ToOpenGLPolygonMode(pipelineDesc.Raster.Fill));

		if (pipelineDesc.DepthStencil.DepthTestEnabled) {
			glEnable(GL_DEPTH_TEST);
		}
		else {
			glDisable(GL_DEPTH_TEST);
		}
		glDepthMask(pipelineDesc.DepthStencil.DepthWriteEnabled ? GL_TRUE : GL_FALSE);
		glDepthFunc(ToOpenGLCompareOp(pipelineDesc.DepthStencil.DepthCompare));

		const auto& colorTarget = pipelineDesc.ColorTargets[0];
		if (colorTarget.BlendEnabled) {
			glEnable(GL_BLEND);
			glBlendFuncSeparate(
				ToOpenGLBlendFactor(colorTarget.SrcColor),
				ToOpenGLBlendFactor(colorTarget.DstColor),
				ToOpenGLBlendFactor(colorTarget.SrcAlpha),
				ToOpenGLBlendFactor(colorTarget.DstAlpha));
			glBlendEquationSeparate(ToOpenGLBlendOp(colorTarget.ColorOp), ToOpenGLBlendOp(colorTarget.AlphaOp));
		}
		else {
			glDisable(GL_BLEND);
		}
		glColorMask(
			(colorTarget.WriteMask & ColorWriteMaskRed) ? GL_TRUE : GL_FALSE,
			(colorTarget.WriteMask & ColorWriteMaskGreen) ? GL_TRUE : GL_FALSE,
			(colorTarget.WriteMask & ColorWriteMaskBlue) ? GL_TRUE : GL_FALSE,
			(colorTarget.WriteMask & ColorWriteMaskAlpha) ? GL_TRUE : GL_FALSE);

		static_cast<OpenGLShaderProgram&>(shaderProgram).BindForCommandList();
		RebuildExplicitVertexArray();
	}

	void OpenGLCommandList::SetVertexBuffer(uint32_t slot, const VertexBufferBinding& binding) {
		if (slot != 0) {
			HE_CORE_WARN("CommandList::SetVertexBuffer skipped because only slot 0 is supported");
			return;
		}

		if (!binding.Buffer || binding.Buffer->GetDesc().Usage != GpuBufferUsage::Vertex) {
			HE_CORE_WARN("CommandList::SetVertexBuffer skipped invalid vertex buffer binding");
			return;
		}

		m_CurrentVertexBufferView = nullptr;
		m_CurrentVertexBufferBinding = binding;
		m_HasExplicitVertexBuffer = true;
		RebuildExplicitVertexArray();
	}

	void OpenGLCommandList::SetIndexBuffer(const IndexBufferBinding& binding) {
		if (!binding.Buffer || binding.Buffer->GetDesc().Usage != GpuBufferUsage::Index || binding.IndexCount == 0) {
			HE_CORE_WARN("CommandList::SetIndexBuffer skipped invalid index buffer binding");
			return;
		}

		if (binding.Format != IndexFormat::UInt32) {
			HE_CORE_WARN("CommandList::SetIndexBuffer skipped unsupported index format");
			return;
		}

		m_CurrentVertexBufferView = nullptr;
		m_CurrentIndexBufferBinding = binding;
		m_HasExplicitIndexBuffer = true;
		RebuildExplicitVertexArray();
	}

	void OpenGLCommandList::SetVertexBufferView(VertexBufferView& vertexBufferView) {
		ReleaseExplicitVertexArray();
		m_HasExplicitVertexBuffer = false;
		m_HasExplicitIndexBuffer = false;
		m_CurrentVertexBufferBinding = {};
		m_CurrentIndexBufferBinding = {};
		m_CurrentVertexBufferView = &vertexBufferView;
		static_cast<OpenGLVertexBufferView&>(vertexBufferView).BindForCommandList();
	}

	void OpenGLCommandList::SetBindGroup(uint32_t slot, BindGroup& bindGroup) {
		if (!m_CurrentShaderProgram) {
			HE_CORE_WARN("CommandList::SetBindGroup skipped because no shader program is bound");
			return;
		}

		if (!m_CurrentPipelineState) {
			HE_CORE_WARN("CommandList::SetBindGroup skipped because no pipeline state is bound");
			return;
		}

		const auto& pipelineDesc = m_CurrentPipelineState->GetDesc();
		const auto layoutIt = std::find_if(
			pipelineDesc.BindGroupLayouts.begin(),
			pipelineDesc.BindGroupLayouts.end(),
			[slot](const PipelineBindGroupLayoutRef& layoutRef) {
				return layoutRef.Slot == slot;
			});
		if (layoutIt == pipelineDesc.BindGroupLayouts.end()) {
			HE_CORE_WARN("CommandList::SetBindGroup skipped because slot {0} is not declared by the current pipeline state", slot);
			return;
		}

		if (!layoutIt->Layout || !bindGroup.GetDesc().Layout) {
			HE_CORE_WARN("CommandList::SetBindGroup skipped because slot {0} has a null layout", slot);
			return;
		}

		if (!BindGroupLayoutEntriesMatch(layoutIt->Layout->GetDesc(), bindGroup.GetDesc().Layout->GetDesc())) {
			HE_CORE_WARN("CommandList::SetBindGroup skipped because slot {0} layout does not match the current pipeline state", slot);
			return;
		}

		auto& shaderProgram = static_cast<OpenGLShaderProgram&>(*m_CurrentShaderProgram);

		if (std::find(m_BoundBindGroupSlots.begin(), m_BoundBindGroupSlots.end(), slot) == m_BoundBindGroupSlots.end()) {
			m_BoundBindGroupSlots.push_back(slot);
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
				else if constexpr (std::is_same_v<T, Ref<TextureView>>) {
					if (!value) {
						HE_CORE_WARN("CommandList::SetBindGroup skipped null texture view binding '{0}'", entry.Name);
						return;
					}

					static_cast<OpenGLTextureView&>(*value).BindForCommandList(entry.TextureSlot);
					shaderProgram.SetInt(entry.Name, static_cast<int>(entry.TextureSlot));
				}
				else if constexpr (std::is_same_v<T, Ref<Sampler>>) {
					if (!value) {
						HE_CORE_WARN("CommandList::SetBindGroup skipped null sampler binding '{0}'", entry.Name);
						return;
					}

					static_cast<OpenGLSampler&>(*value).BindForCommandList(entry.TextureSlot);
				}
			}, entry.Value);
		}
	}

	void OpenGLCommandList::DrawIndexed(uint32_t indexCount) {
		if (!m_CurrentShaderProgram) {
			HE_CORE_WARN("CommandList::DrawIndexed skipped because no shader program is bound");
			return;
		}

		const bool hasVertexBinding = m_CurrentVertexBufferView || m_ExplicitVertexArray != 0;
		if (!hasVertexBinding) {
			HE_CORE_WARN("CommandList::DrawIndexed skipped because no vertex/index binding is active");
			return;
		}

		if (!m_CurrentPipelineState) {
			HE_CORE_WARN("CommandList::DrawIndexed skipped because no pipeline state is bound");
			return;
		}

		if (!m_CurrentRenderTargetStorage) {
			HE_CORE_WARN("CommandList::DrawIndexed skipped because no render target is active");
			return;
		}

		if (!PipelineMatchesCurrentAttachmentFormats(
			m_CurrentPipelineState->GetDesc(),
			m_CurrentColorAttachmentFormat,
			m_CurrentDepthStencilAttachmentFormat)) {
			return;
		}

		for (const auto& layoutRef : m_CurrentPipelineState->GetDesc().BindGroupLayouts) {
			if (std::find(m_BoundBindGroupSlots.begin(), m_BoundBindGroupSlots.end(), layoutRef.Slot) == m_BoundBindGroupSlots.end()) {
				HE_CORE_WARN("CommandList::DrawIndexed skipped because bind group slot {0} is not active", layoutRef.Slot);
				return;
			}
		}

		const uint32_t availableIndexCount = m_CurrentVertexBufferView
			? m_CurrentVertexBufferView->GetDesc().IndexCount
			: m_CurrentIndexBufferBinding.IndexCount;
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

		if (m_ExplicitVertexArray != 0) {
			glBindVertexArray(m_ExplicitVertexArray);
		}

		const auto indexOffset = m_CurrentVertexBufferView
			? 0
			: static_cast<std::uintptr_t>(m_CurrentIndexBufferBinding.Offset);
		glDrawElements(topology, static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT, reinterpret_cast<const void*>(indexOffset));
	}

	void OpenGLCommandList::EndFrame() {
		ReleaseExplicitVertexArray();
		m_CurrentShaderProgram = nullptr;
		m_CurrentPipelineState = nullptr;
		m_CurrentVertexBufferView = nullptr;
		m_BoundBindGroupSlots.clear();
		m_CurrentVertexBufferBinding = {};
		m_CurrentIndexBufferBinding = {};
		m_HasExplicitVertexBuffer = false;
		m_HasExplicitIndexBuffer = false;
	}

	void OpenGLCommandList::EndRenderTarget() {
		EndRenderPass();
	}

	OpenGLCommandBuffer::OpenGLCommandBuffer(const CommandBufferDesc& desc)
		: m_Desc(desc) {
		HE_CORE_ASSERT(m_Desc.Usage == CommandBufferUsage::Graphics, "OpenGL command buffer requires graphics usage");
	}

	const CommandBufferDesc& OpenGLCommandBuffer::GetDesc() const {
		return m_Desc;
	}

	bool OpenGLCommandBuffer::Begin() {
		if (m_IsRecording) {
			HE_CORE_WARN("OpenGLCommandBuffer::Begin skipped because command buffer is already recording");
			return false;
		}

		m_Commands.clear();
		m_RetainedResources.clear();
		m_IsRecording = true;
		m_IsExecutable = false;
		return true;
	}

	bool OpenGLCommandBuffer::End() {
		if (!m_IsRecording) {
			HE_CORE_WARN("OpenGLCommandBuffer::End skipped because command buffer is not recording");
			return false;
		}

		m_IsRecording = false;
		m_IsExecutable = true;
		return true;
	}

	void OpenGLCommandBuffer::Reset() {
		m_Commands.clear();
		m_RetainedResources.clear();
		m_IsRecording = false;
		m_IsExecutable = false;
	}

	bool OpenGLCommandBuffer::IsRecording() const {
		return m_IsRecording;
	}

	bool OpenGLCommandBuffer::IsExecutable() const {
		return m_IsExecutable;
	}

	bool OpenGLCommandBuffer::CanRecord() const {
		if (!m_IsRecording) {
			HE_CORE_WARN("OpenGLCommandBuffer record skipped because command buffer is not recording");
			return false;
		}

		return true;
	}

	bool OpenGLCommandBuffer::RecordBeginRenderPass(const RenderPassDesc& desc) {
		if (!CanRecord()) {
			return false;
		}

		m_Commands.push_back([desc](CommandList& commandList) {
			commandList.BeginRenderPass(desc);
		});
		return true;
	}

	bool OpenGLCommandBuffer::RecordEndRenderPass() {
		if (!CanRecord()) {
			return false;
		}

		m_Commands.push_back([](CommandList& commandList) {
			commandList.EndRenderPass();
		});
		return true;
	}

	bool OpenGLCommandBuffer::RecordResourceBarrier(const ResourceBarrier& barrier) {
		if (!CanRecord()) {
			return false;
		}

		m_Commands.push_back([barrier](CommandList& commandList) {
			commandList.ResourceBarrier(barrier);
		});
		return true;
	}

	bool OpenGLCommandBuffer::RecordBeginFrame() {
		if (!CanRecord()) {
			return false;
		}

		m_Commands.push_back([](CommandList& commandList) {
			commandList.BeginFrame();
		});
		return true;
	}

	bool OpenGLCommandBuffer::RecordSetPipelineState(PipelineState& pipelineState) {
		if (!CanRecord()) {
			return false;
		}

		m_Commands.push_back([pipelineStatePtr = &pipelineState](CommandList& commandList) {
			commandList.SetPipelineState(*pipelineStatePtr);
		});
		return true;
	}

	bool OpenGLCommandBuffer::RecordSetVertexBuffer(uint32_t slot, const VertexBufferBinding& binding) {
		if (!CanRecord()) {
			return false;
		}

		m_Commands.push_back([slot, binding](CommandList& commandList) {
			commandList.SetVertexBuffer(slot, binding);
		});
		return true;
	}

	bool OpenGLCommandBuffer::RecordSetIndexBuffer(const IndexBufferBinding& binding) {
		if (!CanRecord()) {
			return false;
		}

		m_Commands.push_back([binding](CommandList& commandList) {
			commandList.SetIndexBuffer(binding);
		});
		return true;
	}

	bool OpenGLCommandBuffer::RecordSetBindGroup(uint32_t slot, BindGroup& bindGroup) {
		if (!CanRecord()) {
			return false;
		}

		m_Commands.push_back([slot, bindGroupPtr = &bindGroup](CommandList& commandList) {
			commandList.SetBindGroup(slot, *bindGroupPtr);
		});
		return true;
	}

	bool OpenGLCommandBuffer::RecordDrawIndexed(uint32_t indexCount) {
		if (!CanRecord()) {
			return false;
		}

		m_Commands.push_back([indexCount](CommandList& commandList) {
			commandList.DrawIndexed(indexCount);
		});
		return true;
	}

	bool OpenGLCommandBuffer::RecordEndFrame() {
		if (!CanRecord()) {
			return false;
		}

		m_Commands.push_back([](CommandList& commandList) {
			commandList.EndFrame();
		});
		return true;
	}

	void OpenGLCommandBuffer::RetainResource(const std::shared_ptr<void>& resource) {
		if (resource) {
			m_RetainedResources.push_back(resource);
		}
	}

	void OpenGLCommandBuffer::Replay(CommandList& commandList) {
		for (const auto& command : m_Commands) {
			command(commandList);
		}
	}

	OpenGLRenderQueue::OpenGLRenderQueue(CommandList* immediateCommandList)
		: m_ImmediateCommandList(immediateCommandList) {}

	QueueSubmitResult OpenGLRenderQueue::Submit(CommandBuffer& commandBuffer) {
		if (commandBuffer.GetDesc().Usage != CommandBufferUsage::Graphics) {
			HE_CORE_WARN("OpenGL graphics queue skipped non-graphics command buffer");
			return {};
		}

		if (!commandBuffer.IsExecutable()) {
			HE_CORE_WARN("OpenGL graphics queue skipped non-executable command buffer");
			return {};
		}

		if (!m_ImmediateCommandList) {
			HE_CORE_WARN("OpenGL graphics queue skipped command buffer because no immediate command list is available");
			return {};
		}

		auto* openGLCommandBuffer = dynamic_cast<OpenGLCommandBuffer*>(&commandBuffer);
		if (!openGLCommandBuffer) {
			HE_CORE_WARN("OpenGL graphics queue skipped incompatible command buffer");
			return {};
		}

		openGLCommandBuffer->Replay(*m_ImmediateCommandList);
		const uint64_t signalValue = ++m_NextSignalValue;
		m_TimelineFence.Signal(signalValue);
		return {
			.Succeeded = true,
			.SignalValue = signalValue,
			.SignalFence = &m_TimelineFence
		};
	}

	Fence& OpenGLRenderQueue::GetTimelineFence() {
		return m_TimelineFence;
	}

	OpenGLRenderDevice::OpenGLRenderDevice()
		: OpenGLRenderDevice(RenderDeviceDesc{}) {}

	OpenGLRenderDevice::OpenGLRenderDevice(const RenderDeviceDesc& desc)
		: m_Desc(desc), m_GraphicsQueue(&m_ImmediateCommandList) {
		m_Capabilities.Backend = RenderBackendType::OpenGL;
		m_Capabilities.BackendName = "OpenGL";
		m_Capabilities.SupportsPipelineState = true;
		m_Capabilities.SupportsBindGroups = true;
		m_Capabilities.SupportsCommandSubmission = true;
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

	Ref<CommandBuffer> OpenGLRenderDevice::CreateCommandBuffer(const CommandBufferDesc& desc) {
		if (desc.Usage != CommandBufferUsage::Graphics) {
			HE_CORE_ERROR("Invalid command buffer description");
			return nullptr;
		}

		return CreateRef<OpenGLCommandBuffer>(desc);
	}

	RenderQueue& OpenGLRenderDevice::GetGraphicsQueue() {
		return m_GraphicsQueue;
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
		if (!ValidateTextureDesc(desc)) {
			return nullptr;
		}

		return CreateRef<OpenGLTextureResource>(desc);
	}

	Ref<TextureView> OpenGLRenderDevice::CreateTextureView(const TextureViewDesc& desc) {
		if (!desc.Texture) {
			HE_CORE_ERROR("Texture view source texture must not be null");
			return nullptr;
		}

		const auto format = desc.Format == RenderTargetTextureFormat::None ? desc.Texture->GetDesc().Format : desc.Format;
		if (format == RenderTargetTextureFormat::None || desc.MipLevelCount == 0) {
			HE_CORE_ERROR("Invalid texture view description");
			return nullptr;
		}

		TextureViewDesc normalizedDesc = desc;
		normalizedDesc.Format = format;
		return CreateRef<OpenGLTextureView>(normalizedDesc);
	}

	Ref<Sampler> OpenGLRenderDevice::CreateSampler(const SamplerDesc& desc) {
		return CreateRef<OpenGLSampler>(desc);
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
		if (!desc.Shader
			|| desc.VertexLayout.GetElements().empty()
			|| !ValidatePipelineBindGroupLayouts(desc)
			|| !ValidatePipelineRenderState(desc)) {
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
