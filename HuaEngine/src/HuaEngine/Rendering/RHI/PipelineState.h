#pragma once

#include <cstdint>
#include <vector>

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Rendering/RHI/BindGroup.h"
#include "HuaEngine/Rendering/RHI/RenderTargetTypes.h"
#include "HuaEngine/Rendering/RHI/ShaderProgram.h"
#include "HuaEngine/Rendering/VertexLayout.h"

namespace HE::Rendering {
	enum class PrimitiveTopology : uint8_t {
		TriangleList = 0
	};

	enum class BlendFactor : uint8_t {
		Zero = 0,
		One,
		SrcAlpha,
		OneMinusSrcAlpha
	};

	enum class BlendOp : uint8_t {
		Add = 0
	};

	enum class CompareOp : uint8_t {
		Never = 0,
		Less,
		Equal,
		LessEqual,
		Greater,
		NotEqual,
		GreaterEqual,
		Always
	};

	enum class CullMode : uint8_t {
		None = 0,
		Front,
		Back
	};

	enum class FrontFace : uint8_t {
		CounterClockwise = 0,
		Clockwise
	};

	enum class FillMode : uint8_t {
		Solid = 0,
		Wireframe
	};

	constexpr uint8_t ColorWriteMaskRed = 1 << 0;
	constexpr uint8_t ColorWriteMaskGreen = 1 << 1;
	constexpr uint8_t ColorWriteMaskBlue = 1 << 2;
	constexpr uint8_t ColorWriteMaskAlpha = 1 << 3;
	constexpr uint8_t ColorWriteMaskAll = ColorWriteMaskRed | ColorWriteMaskGreen | ColorWriteMaskBlue | ColorWriteMaskAlpha;

	struct PipelineBindGroupLayoutRef {
		uint32_t Slot = 0;
		Ref<BindGroupLayout> Layout;
	};

	struct ColorTargetState {
		RenderTargetTextureFormat Format = RenderTargetTextureFormat::RGBA8;
		bool BlendEnabled = false;
		BlendFactor SrcColor = BlendFactor::One;
		BlendFactor DstColor = BlendFactor::Zero;
		BlendOp ColorOp = BlendOp::Add;
		BlendFactor SrcAlpha = BlendFactor::One;
		BlendFactor DstAlpha = BlendFactor::Zero;
		BlendOp AlphaOp = BlendOp::Add;
		uint8_t WriteMask = ColorWriteMaskAll;
	};

	struct DepthStencilState {
		RenderTargetTextureFormat Format = RenderTargetTextureFormat::DEPTH24_STENCIL8;
		bool DepthTestEnabled = true;
		bool DepthWriteEnabled = true;
		CompareOp DepthCompare = CompareOp::LessEqual;
		bool StencilEnabled = false;
	};

	struct RasterState {
		CullMode Cull = CullMode::Back;
		FrontFace FrontFaceMode = FrontFace::CounterClockwise;
		FillMode Fill = FillMode::Solid;
	};

	struct PipelineStateDesc {
		Ref<ShaderProgram> Shader;
		BufferLayout VertexLayout;
		PrimitiveTopology Topology = PrimitiveTopology::TriangleList;
		std::vector<ColorTargetState> ColorTargets = { ColorTargetState{} };
		DepthStencilState DepthStencil;
		RasterState Raster;
		std::vector<PipelineBindGroupLayoutRef> BindGroupLayouts;
	};

	class PipelineState {
	public:
		virtual ~PipelineState() = default;

		virtual const PipelineStateDesc& GetDesc() const = 0;
	};
}
