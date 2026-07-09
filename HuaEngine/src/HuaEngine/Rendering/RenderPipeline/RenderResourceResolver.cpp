#include "enginepch.h"
#include "RenderResourceResolver.h"

#include "HuaEngine/Asset/AssetResolver.h"
#include "HuaEngine/Asset/AssetTypes.h"
#include "HuaEngine/Rendering/RHI/RenderHardwareInterface.h"
#include "Module/Rendering/RenderingComponent.h"

namespace HE::Rendering {
	namespace {
		void AddDiagnostic(
			std::vector<RenderDiagnostic>& diagnostics,
			RenderDiagnosticCode code,
			Entity sourceEntity,
			std::string message) {
			diagnostics.push_back({ code, sourceEntity, std::move(message) });
		}

		void AddFallbackDiagnostic(
			std::vector<RenderDiagnostic>& diagnostics,
			Entity sourceEntity,
			const AssetGuid& requestedGuid,
			const AssetGuid& fallbackGuid) {
			AddDiagnostic(
				diagnostics,
				RenderDiagnosticCode::FallbackResourceUsed,
				sourceEntity,
				"Asset resolve failed for " + requestedGuid + "; using fallback " + fallbackGuid);
		}

		void AddMaterialBindingParameter(MaterialBinding& binding, const Material& material, const MaterialParameter& parameter) {
			if (parameter.Type == MaterialParameterType::Texture2D) {
				const auto* texture = std::get_if<Ref<TextureResource>>(&parameter.Value);
				if (!texture || !*texture) {
					return;
				}

				binding.Textures.push_back({
					.Name = parameter.Name,
					.Slot = material.GetTextureSlot(parameter.Name),
					.Texture = *texture
				});
				return;
			}

			binding.Parameters.push_back({
				.Name = parameter.Name,
				.Type = parameter.Type,
				.Value = parameter.Value
			});
		}

		Ref<MaterialBinding> BuildMaterialBinding(const MaterialInstance& materialInstance) {
			auto baseMaterial = materialInstance.GetBaseMaterial();
			if (!baseMaterial) {
				return nullptr;
			}

			auto binding = CreateRef<MaterialBinding>();
			for (const auto& [name, parameter] : baseMaterial->GetParameters()) {
				const auto* overrideParameter = materialInstance.GetParameterOverride(name);
				AddMaterialBindingParameter(*binding, *baseMaterial, overrideParameter ? *overrideParameter : parameter);
			}

			for (const auto& [name, parameter] : materialInstance.GetParameterOverrides()) {
				if (baseMaterial->HasParameter(name)) {
					continue;
				}

				AddMaterialBindingParameter(*binding, *baseMaterial, parameter);
			}

			return binding;
		}

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

		void AddMaterialBindGroupEntry(std::vector<BindGroupEntry>& entries, const MaterialParameterValueResolved& parameter) {
			if (parameter.Type == MaterialParameterType::FloatArray ||
				parameter.Type == MaterialParameterType::Texture2D ||
				parameter.Type == MaterialParameterType::TextureCube) {
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
						.Binding = static_cast<uint32_t>(entries.size())
					});
				}
			}, parameter.Value);
		}

		Ref<BindGroup> BuildMaterialBindGroup(const MaterialBinding& binding) {
			std::vector<BindGroupEntry> entries;
			entries.reserve(binding.Parameters.size() + binding.Textures.size());

			for (const auto& parameter : binding.Parameters) {
				AddMaterialBindGroupEntry(entries, parameter);
			}

			for (const auto& texture : binding.Textures) {
				if (!texture.Texture) {
					continue;
				}

				entries.push_back({
					.Name = texture.Name,
					.Type = BindingValueType::Texture,
					.Value = texture.Texture,
					.Binding = static_cast<uint32_t>(entries.size()),
					.TextureSlot = texture.Slot
				});
			}

			if (entries.empty()) {
				return nullptr;
			}

			std::vector<BindGroupLayoutEntry> layoutEntries;
			layoutEntries.reserve(entries.size());
			for (const auto& entry : entries) {
				layoutEntries.push_back({
					.Name = entry.Name,
					.Type = entry.Type,
					.Binding = entry.Binding
				});
			}

			auto& device = RenderHardwareInterface::GetDevice();
			auto layout = device.CreateBindGroupLayout({
				.Scope = BindGroupScope::Material,
				.Entries = std::move(layoutEntries)
			});
			if (!layout) {
				return nullptr;
			}

			return device.CreateBindGroup({
				.Layout = layout,
				.Entries = std::move(entries)
			});
		}
	}

	RenderResourceResolver::RenderResourceResolver(HE::AssetResolver& assetResolver)
		: m_AssetResolver(&assetResolver) {}

	bool RenderResourceResolver::Resolve(
		const RenderItem& item,
		ResolvedRenderItem& outResolvedItem,
		RenderStats& stats,
		std::vector<RenderDiagnostic>& diagnostics) const {
		outResolvedItem = {};
		outResolvedItem.Source = &item;

		if (!m_AssetResolver) {
			AddDiagnostic(
				diagnostics,
				RenderDiagnosticCode::MissingMeshAsset,
				item.SourceEntity,
				"Render resource resolver has no asset resolver");
			return false;
		}

		Ref<Mesh> mesh = nullptr;
		bool usedFallback = false;
		const AssetGuid requestedMeshGuid = item.Mesh.Reference.Guid;
		auto meshResult = m_AssetResolver->ResolveMesh(requestedMeshGuid, mesh);
		if (!meshResult.Succeeded() || !mesh || !mesh->GetVertexBufferView()) {
			Ref<Mesh> fallbackMesh = nullptr;
			const auto fallbackResult = m_AssetResolver->ResolveMesh(BuiltinAssetGuids::FallbackMesh, fallbackMesh);
			if (!fallbackResult.Succeeded() || !fallbackMesh || !fallbackMesh->GetVertexBufferView()) {
				AddDiagnostic(
					diagnostics,
					RenderDiagnosticCode::MissingMeshAsset,
					item.SourceEntity,
					"Fallback mesh asset is unavailable: " + BuiltinAssetGuids::FallbackMesh);
				return false;
			}

			mesh = fallbackMesh;
			usedFallback = true;
			AddFallbackDiagnostic(diagnostics, item.SourceEntity, requestedMeshGuid, BuiltinAssetGuids::FallbackMesh);
		}

		Ref<Material> baseMaterial = nullptr;
		const AssetGuid requestedMaterialGuid = item.Material.Reference.Guid;
		auto materialResult = m_AssetResolver->ResolveMaterial(requestedMaterialGuid, baseMaterial);
		if (!materialResult.Succeeded() || !baseMaterial || !baseMaterial->GetShaderProgram()) {
			Ref<Material> fallbackMaterial = nullptr;
			const auto fallbackResult = m_AssetResolver->ResolveMaterial(BuiltinAssetGuids::FallbackMaterial, fallbackMaterial);
			if (!fallbackResult.Succeeded() || !fallbackMaterial || !fallbackMaterial->GetShaderProgram()) {
				AddDiagnostic(
					diagnostics,
					RenderDiagnosticCode::MissingBaseMaterial,
					item.SourceEntity,
					"Fallback material asset is unavailable: " + BuiltinAssetGuids::FallbackMaterial);
				return false;
			}

			baseMaterial = fallbackMaterial;
			usedFallback = true;
			AddFallbackDiagnostic(diagnostics, item.SourceEntity, requestedMaterialGuid, BuiltinAssetGuids::FallbackMaterial);
		}

		Ref<MaterialInstance> materialInstance = baseMaterial->CreateInstance();
		if (!materialInstance) {
			AddDiagnostic(
				diagnostics,
				RenderDiagnosticCode::MissingMaterialInstance,
				item.SourceEntity,
				"Render item material instance could not be created");
			return false;
		}

		for (const auto& [parameterName, value] : item.MaterialOverrides.Parameters) {
			materialInstance->SetParameter(parameterName, value);
		}

		if (usedFallback) {
			++stats.FallbackItems;
		}

		outResolvedItem.MaterialInstanceRef = materialInstance;
		outResolvedItem.MaterialBindingRef = BuildMaterialBinding(*materialInstance);
		if (outResolvedItem.MaterialBindingRef) {
			outResolvedItem.MaterialBindGroupRef = BuildMaterialBindGroup(*outResolvedItem.MaterialBindingRef);
		}
		outResolvedItem.VertexBufferViewRef = mesh->GetVertexBufferView();
		outResolvedItem.ShaderProgramRef = baseMaterial->GetShaderProgram();
		outResolvedItem.PipelineStateRef = RenderHardwareInterface::GetDevice().CreatePipelineState({
			.Shader = outResolvedItem.ShaderProgramRef,
			.VertexLayout = outResolvedItem.VertexBufferViewRef->GetDesc().Layout,
			.Topology = PrimitiveTopology::TriangleList
		});
		if (!outResolvedItem.PipelineStateRef) {
			AddDiagnostic(
				diagnostics,
				RenderDiagnosticCode::MissingPipelineState,
				item.SourceEntity,
				"Render item pipeline state could not be created");
			return false;
		}

		return true;
	}
}
