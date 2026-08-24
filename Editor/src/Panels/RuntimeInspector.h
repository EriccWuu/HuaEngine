#pragma once

#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

#include "HuaEngine/Reflection/Reflection.h"
#include "Panels/AssetPickerModel.h"
#include "HuaEngine/Rendering/Material/MaterialDefinition.h"
#include "Module/Rendering/RenderingComponent.h"

namespace HE {
	struct AssetImportHealth;
}

namespace HE::Editor {
	struct MaterialNumericEditorOptions {
		float Speed = 0.1f;
		float Minimum = 0.0f;
		float Maximum = 0.0f;
		bool HasRange = false;
	};

	struct RuntimeInspectorContext {
		std::span<const AssetPickerOption> MeshAssets;
		std::span<const AssetPickerOption> MaterialAssets;
		std::span<const AssetPickerOption> TextureAssets;
		std::function<ResultEnvelope(const AssetGuid&, Rendering::MaterialDefinition&, AssetImportHealth&)> ResolveMaterialDefinition;
		std::function<void(const Rendering::MaterialOverrideSet&)> CommitMaterialOverrides;
		std::function<void(const AssetGuid&)> CommitMaterialReference;

		[[nodiscard]] std::span<const AssetPickerOption> GetAssetOptions(AssetKind kind) const {
			switch (kind) {
			case AssetKind::Mesh:
				return MeshAssets;
			case AssetKind::Material:
				return MaterialAssets;
			case AssetKind::Texture2D:
				return TextureAssets;
			case AssetKind::Shader:
			case AssetKind::Unknown:
			default:
				return {};
			}
		}
	};

	using RuntimeComponentEditorOverride =
		std::function<bool(const Refl::RuntimeTypeDescriptor&, void*)>;

	class RuntimeComponentEditorOverrideRegistry {
	public:
		void RegisterOverride(std::string_view qualifiedName, RuntimeComponentEditorOverride editor);
		[[nodiscard]] const RuntimeComponentEditorOverride* FindOverride(std::string_view qualifiedName) const;

	private:
		std::unordered_map<std::string, RuntimeComponentEditorOverride> m_Overrides;
	};

	[[nodiscard]] bool IsRuntimeFieldEditable(const Refl::RuntimeFieldDescriptor& field);
	[[nodiscard]] std::string GetRuntimeComponentDisplayName(const Refl::RuntimeTypeDescriptor& type);
	[[nodiscard]] MaterialNumericEditorOptions GetMaterialNumericEditorOptions(
		const Rendering::MaterialParameterDefinition& parameter);

	bool DrawRuntimeFieldEditor(
		const Refl::RuntimeFieldDescriptor& field,
		void* component,
		RuntimeInspectorContext context = {});
	bool DrawRuntimeComponentInspector(
		const Refl::RuntimeTypeDescriptor& type,
		void* component,
		const RuntimeComponentEditorOverrideRegistry& overrides,
		RuntimeInspectorContext context = {});
}
