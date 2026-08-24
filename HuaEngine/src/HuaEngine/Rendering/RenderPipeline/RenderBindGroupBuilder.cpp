#include "enginepch.h"
#include "RenderBindGroupBuilder.h"

#include <cstring>
#include <type_traits>

#include "HuaEngine/Rendering/Material/MaterialCore.h"
#include "HuaEngine/Rendering/RHI/RenderDevice.h"
#include "HuaEngine/Rendering/RHI/ShaderProgram.h"
#include "HuaEngine/Rendering/RenderPipeline/UniformBufferArena.h"

namespace HE::Rendering {
	namespace {
		Ref<BindGroup> CreateMatrixBindGroup(RenderDevice& device, UniformBufferArena& arena, const ShaderConstantBuffer& block, Ref<BindGroupLayout> layout, const char* memberName, const glm::mat4& value) {
			if (!layout || block.Size == 0) return nullptr;
			const auto member = std::find_if(block.Members.begin(), block.Members.end(), [&](const auto& candidate) { return candidate.Name == memberName; });
			if (member == block.Members.end() || member->Size != sizeof(glm::mat4) || member->Offset > block.Size || member->Size > block.Size - member->Offset) return nullptr;

			std::vector<uint8_t> constants(block.Size);
			std::memcpy(constants.data() + member->Offset, &value[0][0], sizeof(glm::mat4));
			UniformBufferAllocation allocation;
			if (!arena.Allocate(constants.data(), block.Size, allocation)) return nullptr;
			return device.CreateBindGroup({
				.Layout = std::move(layout),
				.Entries = {{ .Name = block.Name, .Type = BindingValueType::UniformBuffer, .Value = allocation.Buffer, .Binding = block.Binding, .Offset = allocation.Offset, .Size = block.Size }}
			});
		}

		void AddTextureEntry(std::vector<BindGroupEntry>& entries, const MaterialParameter& parameter, const ShaderResourceBinding& texture) {
			const auto* resource = std::get_if<Ref<TextureResource>>(&parameter.Value);
			if (!resource || !*resource) return;
			entries.push_back({ .Name = texture.Name, .Type = BindingValueType::Texture, .Value = *resource, .Binding = texture.Binding });
		}
	}

	Ref<BindGroupLayout> CreateUniformBlockBindGroupLayout(RenderDevice& device, BindGroupScope scope, const ShaderConstantBuffer& block, ShaderStageFlags visibility, const Sha256Digest& interfaceDigest) {
		if (block.Size == 0) return nullptr;
		return device.CreateBindGroupLayout({
			.Scope = scope,
			.Entries = {{ .Name = block.Name, .Type = BindingValueType::UniformBuffer, .Binding = block.Binding, .Visibility = visibility, .MinBindingSize = block.Size }},
			.InterfaceDigest = interfaceDigest
		});
	}

	Ref<BindGroupLayout> CreateMaterialBindGroupLayout(RenderDevice& device, const ShaderConstantBuffer& block, ShaderStageFlags blockVisibility, const std::vector<ShaderResourceBinding>& textures, const Sha256Digest& interfaceDigest) {
		if (block.Size == 0) return nullptr;
		std::vector<BindGroupLayoutEntry> entries = {{ .Name = block.Name, .Type = BindingValueType::UniformBuffer, .Binding = block.Binding, .Visibility = blockVisibility, .MinBindingSize = block.Size }};
		for (const auto& texture : textures) entries.push_back({ .Name = texture.Name, .Type = BindingValueType::Texture, .Binding = texture.Binding, .Visibility = texture.StageMask });
		return device.CreateBindGroupLayout({ .Scope = BindGroupScope::Material, .Entries = std::move(entries), .InterfaceDigest = interfaceDigest });
	}

	Ref<BindGroup> CreateFrameBindGroup(RenderDevice& device, UniformBufferArena& arena, const ShaderConstantBuffer& block, Ref<BindGroupLayout> layout, const glm::mat4& viewProjection) {
		return CreateMatrixBindGroup(device, arena, block, std::move(layout), "u_ViewProjection", viewProjection);
	}

	Ref<BindGroup> CreateObjectBindGroup(RenderDevice& device, UniformBufferArena& arena, const ShaderConstantBuffer& block, Ref<BindGroupLayout> layout, const glm::mat4& transform) {
		return CreateMatrixBindGroup(device, arena, block, std::move(layout), "u_Transform", transform);
	}

	Ref<BindGroup> CreateMaterialBindGroup(RenderDevice& device, UniformBufferArena& arena, const MaterialInstance& materialInstance, const ShaderConstantBuffer& block, const std::vector<ShaderResourceBinding>& textures, Ref<BindGroupLayout> layout) {
		auto baseMaterial = materialInstance.GetBaseMaterial();
		if (!baseMaterial || !layout || block.Size == 0) return nullptr;

		std::vector<BindGroupEntry> entries;
		std::vector<uint8_t> constants(block.Size);
		for (const auto& member : block.Members) {
			const auto* baseParameter = baseMaterial->GetParameter(member.Name);
			if (!baseParameter) continue;
			const auto* overrideParameter = materialInstance.GetParameterOverride(member.Name);
			const auto& parameter = overrideParameter ? *overrideParameter : *baseParameter;
			if (member.Offset > constants.size() || member.Size > constants.size() - member.Offset) return nullptr;
			std::visit([&](const auto& value) {
				using T = std::decay_t<decltype(value)>;
				if constexpr (!std::is_same_v<T, std::vector<float>> && !std::is_same_v<T, Ref<TextureResource>>) {
					if (sizeof(T) >= member.Size) std::memcpy(constants.data() + member.Offset, &value, member.Size);
				}
			}, parameter.Value);
		}
		for (const auto& texture : textures) {
			const auto* baseParameter = baseMaterial->GetParameter(texture.Name);
			if (!baseParameter) continue;
			const auto* overrideParameter = materialInstance.GetParameterOverride(texture.Name);
			AddTextureEntry(entries, overrideParameter ? *overrideParameter : *baseParameter, texture);
		}

		UniformBufferAllocation allocation;
		if (!arena.Allocate(constants.data(), block.Size, allocation)) return nullptr;
		entries.insert(entries.begin(), { .Name = block.Name, .Type = BindingValueType::UniformBuffer, .Value = allocation.Buffer, .Binding = block.Binding, .Offset = allocation.Offset, .Size = block.Size });
		return device.CreateBindGroup({ .Layout = std::move(layout), .Entries = std::move(entries) });
	}
}
