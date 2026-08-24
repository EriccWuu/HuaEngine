#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Rendering/RHI/BindGroup.h"
#include "HuaEngine/Rendering/Shader/ShaderInterface.h"
#include "glm/glm.hpp"

#include <vector>

namespace HE::Rendering {
	class BindGroup;
	class BindGroupLayout;
	class MaterialInstance;
	class RenderDevice;
	class UniformBufferArena;
	Ref<BindGroupLayout> CreateUniformBlockBindGroupLayout(RenderDevice& device, BindGroupScope scope, const ShaderConstantBuffer& block, ShaderStageFlags visibility, const Sha256Digest& interfaceDigest);
	Ref<BindGroupLayout> CreateMaterialBindGroupLayout(RenderDevice& device, const ShaderConstantBuffer& block, ShaderStageFlags blockVisibility, const std::vector<ShaderResourceBinding>& textures, const Sha256Digest& interfaceDigest);
	Ref<BindGroup> CreateFrameBindGroup(RenderDevice& device, UniformBufferArena& arena, const ShaderConstantBuffer& block, Ref<BindGroupLayout> layout, const glm::mat4& viewProjection);
	Ref<BindGroup> CreateObjectBindGroup(RenderDevice& device, UniformBufferArena& arena, const ShaderConstantBuffer& block, Ref<BindGroupLayout> layout, const glm::mat4& transform);
	Ref<BindGroup> CreateMaterialBindGroup(RenderDevice& device, UniformBufferArena& arena, const MaterialInstance& materialInstance, const ShaderConstantBuffer& block, const std::vector<ShaderResourceBinding>& textures, Ref<BindGroupLayout> layout);
}
