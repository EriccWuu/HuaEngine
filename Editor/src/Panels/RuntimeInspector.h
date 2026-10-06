#pragma once

#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "HuaEngine/Reflection/Reflection.h"
#include "Assets/AssetPickerModel.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Rendering/Material/MaterialDefinition.h"

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
		std::span<const AssetPickerOption> ShaderAssets;
		std::function<ResultEnvelope(const AssetGuid&, Rendering::MaterialDefinition&, AssetImportHealth&)> ResolveMaterialDefinition;
		std::vector<ResultEnvelope>* DeferredEvents = nullptr;

		[[nodiscard]] std::span<const AssetPickerOption> GetAssetOptions(AssetKind kind) const {
			switch (kind) {
			case AssetKind::Mesh:
				return MeshAssets;
			case AssetKind::Material:
				return MaterialAssets;
			case AssetKind::Texture2D:
				return TextureAssets;
			case AssetKind::Shader:
					return ShaderAssets;
			case AssetKind::Unknown:
			default:
				return {};
			}
		}
	};

	using RuntimeFieldDrawer = std::function<bool(
		const Refl::RuntimeFieldDescriptor&,
		const Refl::RuntimeTypeDescriptor&,
		void* object,
		void* value,
		RuntimeInspectorContext&)>;

	class RuntimeFieldDrawerRegistry {
	public:
		RuntimeFieldDrawerRegistry();
		void RegisterNamed(std::string_view name, RuntimeFieldDrawer drawer);
		void RegisterKind(Refl::RuntimeFieldValueKind kind, RuntimeFieldDrawer drawer);
		[[nodiscard]] const RuntimeFieldDrawer* Resolve(const Refl::RuntimeFieldDescriptor& field) const;

	private:
		std::map<std::string, RuntimeFieldDrawer, std::less<>> m_Named;
		std::map<Refl::RuntimeFieldValueKind, RuntimeFieldDrawer> m_ByKind;
		RuntimeFieldDrawer m_AssetReference;
	};

	[[nodiscard]] bool IsRuntimeFieldEditable(const Refl::RuntimeFieldDescriptor& field);
	[[nodiscard]] std::string GetRuntimeComponentDisplayName(const Refl::RuntimeTypeDescriptor& type);
	[[nodiscard]] MaterialNumericEditorOptions GetMaterialNumericEditorOptions(
		const Rendering::MaterialParameterDefinition& parameter);
	bool SetRuntimeMaterialReference(
		const Refl::RuntimeFieldDescriptor& field,
		const Refl::RuntimeTypeDescriptor& type,
		void* object,
		const AssetGuid& nextGuid,
		RuntimeInspectorContext& context);

	bool DrawRuntimeFields(
		const Refl::RuntimeTypeDescriptor& type,
		void* object,
		const RuntimeFieldDrawerRegistry& drawers,
		RuntimeInspectorContext context = {});
}
