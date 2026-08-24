#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Rendering/RHI/BindGroup.h"
#include "glm/glm.hpp"

#include <vector>

namespace HE::Rendering {
	class BindGroup;
	class BindGroupLayout;
	class MaterialInstance;
	class RenderDevice;
	class UniformBufferArena;
	struct ShaderTextureBinding;
	struct ShaderUniformBlockBinding;

	Ref<BindGroupLayout> CreateUniformBlockBindGroupLayout(RenderDevice& device, BindGroupScope scope, const ShaderUniformBlockBinding& block);
	Ref<BindGroupLayout> CreateMaterialBindGroupLayout(RenderDevice& device, const ShaderUniformBlockBinding& block, const std::vector<ShaderTextureBinding>& textures);
	Ref<BindGroup> CreateFrameBindGroup(RenderDevice& device, UniformBufferArena& arena, const ShaderUniformBlockBinding& block, Ref<BindGroupLayout> layout, const glm::mat4& viewProjection);
	Ref<BindGroup> CreateObjectBindGroup(RenderDevice& device, UniformBufferArena& arena, const ShaderUniformBlockBinding& block, Ref<BindGroupLayout> layout, const glm::mat4& transform);
	Ref<BindGroup> CreateMaterialBindGroup(RenderDevice& device, UniformBufferArena& arena, const MaterialInstance& materialInstance, const ShaderUniformBlockBinding& block, const std::vector<ShaderTextureBinding>& textures, Ref<BindGroupLayout> layout);
}
