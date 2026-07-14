#pragma once

#include "HuaEngine/Core/Core.h"
#include "glm/glm.hpp"

namespace HE::Rendering {
	class BindGroup;
	class MaterialInstance;
	class RenderDevice;

	Ref<BindGroup> CreateFrameBindGroup(RenderDevice& device, const glm::mat4& viewProjection);
	Ref<BindGroup> CreateObjectBindGroup(RenderDevice& device, const glm::mat4& transform);
	Ref<BindGroup> CreateMaterialBindGroup(RenderDevice& device, const MaterialInstance& materialInstance);
}
