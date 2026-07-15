#pragma once

#include <cstdint>
#include <vector>

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Rendering/RHI/BindGroup.h"
#include "HuaEngine/Rendering/RHI/ShaderProgram.h"
#include "HuaEngine/Rendering/VertexLayout.h"

namespace HE::Rendering {
	enum class PrimitiveTopology : uint8_t {
		TriangleList = 0
	};

	struct PipelineBindGroupLayoutRef {
		uint32_t Slot = 0;
		Ref<BindGroupLayout> Layout;
	};

	struct PipelineStateDesc {
		Ref<ShaderProgram> Shader;
		BufferLayout VertexLayout;
		PrimitiveTopology Topology = PrimitiveTopology::TriangleList;
		std::vector<PipelineBindGroupLayoutRef> BindGroupLayouts;
	};

	class PipelineState {
	public:
		virtual ~PipelineState() = default;

		virtual const PipelineStateDesc& GetDesc() const = 0;
	};
}
