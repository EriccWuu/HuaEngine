#pragma once

#include <cstdint>
#include <optional>
#include <vector>
#include "glm/glm.hpp"
#include "HuaEngine/Core/Core.h"

namespace HE::Rendering {
	class RenderTarget;

	enum class LoadOp : uint8_t {
		Load = 0,
		Clear,
		DontCare
	};

	enum class StoreOp : uint8_t {
		Store = 0,
		DontCare
	};

	struct RenderPassColorAttachment {
		Ref<RenderTarget> Target;
		uint32_t AttachmentIndex = 0;
		LoadOp Load = LoadOp::Clear;
		StoreOp Store = StoreOp::Store;
		glm::vec4 ClearColor = glm::vec4(0.0f);
	};

	struct RenderPassDepthStencilAttachment {
		Ref<RenderTarget> Target;
		LoadOp DepthLoad = LoadOp::Clear;
		StoreOp DepthStore = StoreOp::Store;
		float ClearDepth = 1.0f;
		uint32_t ClearStencil = 0;
	};

	struct RenderPassDesc {
		std::vector<RenderPassColorAttachment> ColorAttachments;
		std::optional<RenderPassDepthStencilAttachment> DepthStencilAttachment;
	};
}
