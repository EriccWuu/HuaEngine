#pragma once

#include "HuaEngine/Core/Core.h"
#include "glm/glm.hpp"

namespace HE::Rendering {
	class BindGroup;
	class BindGroupLayout;
	struct MaterialBindingSchema;
	class MaterialInstance;
	class RenderDevice;

	Ref<BindGroupLayout> CreateFrameBindGroupLayout(RenderDevice& device);
	Ref<BindGroupLayout> CreateObjectBindGroupLayout(RenderDevice& device);
	Ref<BindGroup> CreateFrameBindGroup(RenderDevice& device, const glm::mat4& viewProjection);
	Ref<BindGroup> CreateObjectBindGroup(RenderDevice& device, const glm::mat4& transform);
	Ref<BindGroupLayout> CreateMaterialBindGroupLayout(RenderDevice& device, const MaterialBindingSchema& schema);
	Ref<BindGroup> CreateMaterialBindGroup(RenderDevice& device, const MaterialInstance& materialInstance);
	Ref<BindGroup> CreateMaterialBindGroup(RenderDevice& device, const MaterialInstance& materialInstance, Ref<BindGroupLayout> layout);
}
