#include "enginepch.h"
#include "RenderBindGroupBuilder.h"

#include <type_traits>
#include <vector>

#include "HuaEngine/Rendering/Material/MaterialCore.h"
#include "HuaEngine/Rendering/RHI/BindGroup.h"
#include "HuaEngine/Rendering/RHI/RenderDevice.h"

namespace HE::Rendering {
	namespace {
		BindingValueType ToBindingValueType(MaterialParameterType type) {
			switch (type) {
				case MaterialParameterType::Int:
					return BindingValueType::Int;
				case MaterialParameterType::Float:
					return BindingValueType::Float;
				case MaterialParameterType::Vec2:
					return BindingValueType::Float2;
				case MaterialParameterType::Vec3:
					return BindingValueType::Float3;
				case MaterialParameterType::Vec4:
					return BindingValueType::Float4;
				case MaterialParameterType::Mat3:
					return BindingValueType::Mat3;
				case MaterialParameterType::Mat4:
					return BindingValueType::Mat4;
				case MaterialParameterType::IntArray:
					return BindingValueType::IntArray;
				case MaterialParameterType::Texture2D:
				case MaterialParameterType::TextureCube:
					return BindingValueType::Texture;
				case MaterialParameterType::FloatArray:
					break;
			}

			return BindingValueType::Float;
		}

		Ref<BindGroup> CreateSingleMat4BindGroup(
			RenderDevice& device,
			Ref<BindGroupLayout> layout,
			const char* name,
			const glm::mat4& value) {
			if (!layout) {
				return nullptr;
			}

			return device.CreateBindGroup({
				.Layout = layout,
				.Entries = {
					{
						.Name = name,
						.Type = BindingValueType::Mat4,
						.Value = value,
						.Binding = 0
					}
				}
			});
		}

		Ref<BindGroupLayout> CreateSingleMat4BindGroupLayout(
			RenderDevice& device,
			BindGroupScope scope,
			const char* name) {
			return device.CreateBindGroupLayout({
				.Scope = scope,
				.Entries = {
					{
						.Name = name,
						.Type = BindingValueType::Mat4,
						.Binding = 0
					}
				}
			});
		}

		void AddMaterialParameterEntry(
			std::vector<BindGroupEntry>& entries,
			const MaterialParameter& parameter,
			uint32_t binding,
			uint32_t textureSlot) {
			if (parameter.Type == MaterialParameterType::FloatArray) {
				return;
			}

			if (parameter.Type == MaterialParameterType::Texture2D || parameter.Type == MaterialParameterType::TextureCube) {
				const auto* texture = std::get_if<Ref<TextureResource>>(&parameter.Value);
				if (!texture || !*texture) {
					return;
				}

				entries.push_back({
					.Name = parameter.Name,
					.Type = BindingValueType::Texture,
					.Value = *texture,
					.Binding = binding,
					.TextureSlot = textureSlot
				});
				return;
			}

			std::visit([&](auto&& value) {
				using T = std::decay_t<decltype(value)>;

				if constexpr (std::is_same_v<T, std::vector<float>> || std::is_same_v<T, Ref<TextureResource>>) {
					return;
				}
				else {
					entries.push_back({
						.Name = parameter.Name,
						.Type = ToBindingValueType(parameter.Type),
						.Value = value,
						.Binding = binding
					});
				}
			}, parameter.Value);
		}
	}

	Ref<BindGroupLayout> CreateFrameBindGroupLayout(RenderDevice& device) {
		return CreateSingleMat4BindGroupLayout(device, BindGroupScope::Frame, "u_ViewProjection");
	}

	Ref<BindGroupLayout> CreateObjectBindGroupLayout(RenderDevice& device) {
		return CreateSingleMat4BindGroupLayout(device, BindGroupScope::Object, "u_Transform");
	}

	Ref<BindGroup> CreateFrameBindGroup(RenderDevice& device, const glm::mat4& viewProjection) {
		return CreateSingleMat4BindGroup(device, CreateFrameBindGroupLayout(device), "u_ViewProjection", viewProjection);
	}

	Ref<BindGroup> CreateObjectBindGroup(RenderDevice& device, const glm::mat4& transform) {
		return CreateSingleMat4BindGroup(device, CreateObjectBindGroupLayout(device), "u_Transform", transform);
	}

	Ref<BindGroupLayout> CreateMaterialBindGroupLayout(RenderDevice& device, const MaterialBindingSchema& schema) {
		if (schema.Entries.empty()) {
			return nullptr;
		}

		std::vector<BindGroupLayoutEntry> layoutEntries;
		layoutEntries.reserve(schema.Entries.size());
		for (const auto& entry : schema.Entries) {
			layoutEntries.push_back({
				.Name = entry.Name,
				.Type = ToBindingValueType(entry.Type),
				.Binding = entry.Binding
			});
		}

		return device.CreateBindGroupLayout({
			.Scope = BindGroupScope::Material,
			.Entries = std::move(layoutEntries)
		});
	}

	Ref<BindGroup> CreateMaterialBindGroup(RenderDevice& device, const MaterialInstance& materialInstance) {
		auto baseMaterial = materialInstance.GetBaseMaterial();
		if (!baseMaterial) {
			return nullptr;
		}

		return CreateMaterialBindGroup(device, materialInstance, CreateMaterialBindGroupLayout(device, baseMaterial->GetBindingSchema()));
	}

	Ref<BindGroup> CreateMaterialBindGroup(RenderDevice& device, const MaterialInstance& materialInstance, Ref<BindGroupLayout> layout) {
		auto baseMaterial = materialInstance.GetBaseMaterial();
		if (!baseMaterial || !layout) {
			return nullptr;
		}

		const auto schema = baseMaterial->GetBindingSchema();
		std::vector<BindGroupEntry> entries;
		entries.reserve(schema.Entries.size());

		for (const auto& schemaEntry : schema.Entries) {
			const auto* baseParameter = baseMaterial->GetParameter(schemaEntry.Name);
			if (!baseParameter) {
				continue;
			}

			const auto* overrideParameter = materialInstance.GetParameterOverride(schemaEntry.Name);
			const auto& selectedParameter = overrideParameter ? *overrideParameter : *baseParameter;
			AddMaterialParameterEntry(entries, selectedParameter, schemaEntry.Binding, schemaEntry.TextureSlot);
		}

		if (entries.empty()) {
			return nullptr;
		}

		return device.CreateBindGroup({
			.Layout = layout,
			.Entries = std::move(entries)
		});
	}
}
