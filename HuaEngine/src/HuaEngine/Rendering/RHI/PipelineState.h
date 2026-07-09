#pragma once

#include <cstdint>

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Rendering/RHI/ShaderProgram.h"
#include "HuaEngine/Rendering/VertexLayout.h"

namespace HE::Rendering {
	enum class PrimitiveTopology : uint8_t {
		TriangleList = 0
	};

	struct PipelineStateDesc {
		Ref<ShaderProgram> Shader;
		BufferLayout VertexLayout;
		PrimitiveTopology Topology = PrimitiveTopology::TriangleList;
	};

	class PipelineState {
	public:
		virtual ~PipelineState() = default;

		virtual const PipelineStateDesc& GetDesc() const = 0;
	};
}
