#include "RuntimeInspector.h"
#include "Module/Rendering/RenderingComponent.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstring>
#include <optional>
#include <utility>

#include "glm/glm.hpp"
#include "imgui.h"
#include "HuaEngine/Asset/AssetService.h"
#include "HuaEngine/Asset/AssetTypes.h"

namespace HE::Editor {
	namespace {
		int ResizeStringInputCallback(ImGuiInputTextCallbackData* data) {
			if (data->EventFlag != ImGuiInputTextFlags_CallbackResize) {
				return 0;
			}

			auto* text = static_cast<std::string*>(data->UserData);
			text->resize(static_cast<size_t>(data->BufTextLen));
			data->Buf = text->data();
			return 0;
		}

		const char* FieldLabel(const Refl::RuntimeFieldDescriptor& field) {
			return field.DisplayName.empty() ? field.Name.data() : field.DisplayName.data();
		}

		bool BeginRuntimeFieldTable(const char* id) {
			const float availableWidth = ImGui::GetContentRegionAvail().x;
			const float labelWidth = std::clamp(availableWidth * 0.28f, 72.0f, 112.0f);
			const ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp |
				ImGuiTableFlags_NoSavedSettings |
				ImGuiTableFlags_NoPadOuterX;
			if (!ImGui::BeginTable(id, 2, flags)) {
				return false;
			}
			ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, labelWidth);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
			return true;
		}

		ImGuiDataType ScalarTypeForSignedField(const Refl::RuntimeFieldDescriptor& field) {
			switch (field.Size) {
				case sizeof(int8_t):
					return ImGuiDataType_S8;
				case sizeof(int16_t):
					return ImGuiDataType_S16;
				case sizeof(int64_t):
					return ImGuiDataType_S64;
				case sizeof(int32_t):
				default:
					return ImGuiDataType_S32;
			}
		}

		ImGuiDataType ScalarTypeForUnsignedField(const Refl::RuntimeFieldDescriptor& field) {
			switch (field.Size) {
				case sizeof(uint8_t):
					return ImGuiDataType_U8;
				case sizeof(uint16_t):
					return ImGuiDataType_U16;
				case sizeof(uint64_t):
					return ImGuiDataType_U64;
				case sizeof(uint32_t):
				default:
					return ImGuiDataType_U32;
			}
		}

		std::optional<std::pair<float, float>> FloatEditorRange(const Refl::RuntimeFieldDescriptor& field) {
			const auto* minimum = Refl::FindRuntimeAttribute(field.Attributes, "Editor.Min");
			const auto* maximum = Refl::FindRuntimeAttribute(field.Attributes, "Editor.Max");
			if (!minimum || !maximum) return std::nullopt;
			float lower = 0.0f;
			float upper = 0.0f;
			const auto low = std::from_chars(minimum->Value.data(), minimum->Value.data() + minimum->Value.size(), lower);
			const auto high = std::from_chars(maximum->Value.data(), maximum->Value.data() + maximum->Value.size(), upper);
			if (low.ec != std::errc{} || low.ptr != minimum->Value.data() + minimum->Value.size() ||
				high.ec != std::errc{} || high.ptr != maximum->Value.data() + maximum->Value.size() ||
				!std::isfinite(lower) || !std::isfinite(upper) || lower >= upper) return std::nullopt;
			return std::pair{lower, upper};
		}

		bool DrawRuntimeEnumField(const Refl::RuntimeFieldDescriptor& field, void* component) {
			if (field.EnumType == nullptr) {
				ImGui::TextDisabled("Enum metadata unavailable");
				return false;
			}

			int64_t currentValue = 0;
			if (!Refl::GetRuntimeEnumFieldValue(field, component, currentValue)) {
				ImGui::TextDisabled("Enum value unavailable");
				return false;
			}

			const Refl::RuntimeEnumValueDescriptor* current =
				Refl::FindRuntimeEnumValueByValue(*field.EnumType, currentValue);
			const char* preview = current != nullptr
				? (current->DisplayName.empty() ? current->Name.data() : current->DisplayName.data())
				: "<unknown>";

			bool changed = false;
			if (ImGui::BeginCombo("##Value", preview)) {
				for (const Refl::RuntimeEnumValueDescriptor& value : field.EnumType->Values) {
					const bool selected = value.Value == currentValue;
					const char* itemLabel = value.DisplayName.empty() ? value.Name.data() : value.DisplayName.data();
					if (ImGui::Selectable(itemLabel, selected)) {
						changed = Refl::SetRuntimeEnumFieldValue(field, component, value.Value);
					}
					if (selected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			return changed;
		}

		AssetGuid* GetAssetRefGuid(const Refl::RuntimeFieldDescriptor& field, void* value) {
			if (value == nullptr) return nullptr;
			if (field.Type == "MeshAssetRef") {
				return &static_cast<MeshAssetRef*>(value)->Reference.Guid;
			}
			if (field.Type == "MaterialAssetRef") {
				return &static_cast<MaterialAssetRef*>(value)->Reference.Guid;
			}
			if (field.Type == "TextureAssetRef") {
				return &static_cast<TextureAssetRef*>(value)->Reference.Guid;
			}
			return nullptr;
		}

		const char* AssetKindDisplayName(AssetKind kind) {
			switch (kind) {
			case AssetKind::Mesh:
				return "mesh";
			case AssetKind::Material:
				return "material";
			case AssetKind::Texture2D:
				return "texture";
			case AssetKind::Shader:
				return "shader";
			case AssetKind::Unknown:
			default:
				return "asset";
			}
		}

		bool DrawAssetRefField(
			AssetGuid& guid,
			AssetKind kind,
			std::span<const AssetPickerOption> options) {
			const AssetPickerPreview preview = GetAssetPickerPreview(options, guid);
			bool changed = false;
			ImGui::SetNextItemWidth(-1.0f);
			const bool comboOpen = ImGui::BeginCombo("##AssetValue", preview.DisplayName.c_str(), ImGuiComboFlags_HeightLarge);
			const bool comboHovered = ImGui::IsItemHovered();
			if (comboOpen) {
				static std::array<char, 128> searchBuffer{};
				if (ImGui::IsWindowAppearing()) {
					searchBuffer.fill('\0');
				}

				ImGui::SetNextItemWidth(-1.0f);
				const std::string searchHint = std::string("Search ") + AssetKindDisplayName(kind) + " assets...";
				ImGui::InputTextWithHint("##AssetSearch", searchHint.c_str(), searchBuffer.data(), searchBuffer.size());
				ImGui::Separator();

				const bool noneSelected = guid.empty();
				if (ImGui::Selectable("None", noneSelected)) {
					guid.clear();
					changed = true;
				}
				if (noneSelected) {
					ImGui::SetItemDefaultFocus();
				}

				bool hasMatchingAsset = false;
				for (const AssetPickerOption& option : options) {
					if (!AssetPickerOptionMatches(option, searchBuffer.data())) {
						continue;
					}

					hasMatchingAsset = true;
					ImGui::PushID(option.Guid.c_str());
					const bool selected = option.Guid == guid;
					if (ImGui::Selectable(option.DisplayName.c_str(), selected)) {
						guid = option.Guid;
						changed = true;
					}
					if (selected) {
						ImGui::SetItemDefaultFocus();
					}
					ImGui::PopID();
				}

				if (!hasMatchingAsset) {
					ImGui::TextDisabled("No matching %s assets", AssetKindDisplayName(kind));
				}
				ImGui::EndCombo();
			}

			if (comboHovered) {
				if (preview.Missing) {
					ImGui::SetTooltip("%s asset is not present in the current project: %s", AssetKindDisplayName(kind), guid.c_str());
				}
				else {
					ImGui::SetTooltip("%s", preview.DisplayName.c_str());
				}
			}
			return changed;
		}

		bool DrawRuntimeAssetRefField(
			const Refl::RuntimeFieldDescriptor& field,
			void* value,
			RuntimeInspectorContext& context) {
			AssetGuid* guid = GetAssetRefGuid(field, value);
			if (guid == nullptr) {
				ImGui::TextDisabled("Unsupported asset ref: %.*s", static_cast<int>(field.Type.size()), field.Type.data());
				return false;
			}
			const auto* assetKind = Refl::FindRuntimeAttribute(field.Attributes, "Editor.AssetKind");
			const AssetKind kind = assetKind ? AssetKindFromString(assetKind->Value) : AssetKind::Unknown;
			if (assetKind && kind == AssetKind::Unknown) {
				ImGui::TextDisabled("Unknown asset kind: %.*s",
					static_cast<int>(assetKind->Value.size()), assetKind->Value.data());
				return false;
			}
			if (kind != AssetKind::Unknown) {
				return DrawAssetRefField(*guid, kind, context.GetAssetOptions(kind));
			}

			std::array<char, 256> editedGuid{};
			const size_t copyLength = std::min(guid->size(), editedGuid.size() - 1);
			std::memcpy(editedGuid.data(), guid->data(), copyLength);
			const bool changed = ImGui::InputText(
				"##Value",
				editedGuid.data(),
				editedGuid.size());
			if (changed) {
				*guid = editedGuid.data();
			}
			return changed;
		}

		const Refl::RuntimeFieldDescriptor* FindField(
			const Refl::RuntimeTypeDescriptor& type,
			std::string_view name) {
			for (const auto& field : type.Fields) {
				if (field.Name == name) return &field;
			}
			return nullptr;
		}

		bool DrawMaterialReference(
			const Refl::RuntimeFieldDescriptor& field,
			const Refl::RuntimeTypeDescriptor& type,
			void* object,
			void* value,
			RuntimeInspectorContext& context) {
			AssetGuid* guid = GetAssetRefGuid(field, value);
			if (field.Type != "MaterialAssetRef" || guid == nullptr) {
				ImGui::TextDisabled("Material reference unavailable");
				return false;
			}
			const auto* reconcileAttribute = Refl::FindRuntimeAttribute(field.Attributes, "Editor.ReconcileField");
			const auto* reconcileField = reconcileAttribute ? FindField(type, reconcileAttribute->Value) : nullptr;
			if (!reconcileField || reconcileField->Type != "MaterialOverrideSet" ||
				Refl::IsRuntimeFieldReadOnly(*reconcileField) ||
				Refl::GetRuntimeFieldMutable(*reconcileField, object) == nullptr) {
				ImGui::TextDisabled("Material override field unavailable");
				return false;
			}

			AssetGuid nextGuid = *guid;
			const auto* assetKind = Refl::FindRuntimeAttribute(field.Attributes, "Editor.AssetKind");
			const AssetKind kind = assetKind ? AssetKindFromString(assetKind->Value) : AssetKind::Unknown;
			if (kind != AssetKind::Material) {
				ImGui::TextDisabled("Material asset kind unavailable");
				return false;
			}
			if (!DrawAssetRefField(nextGuid, kind, context.GetAssetOptions(kind))) {
				return false;
			}
			return SetRuntimeMaterialReference(field, type, object, nextGuid, context);
		}

		bool DrawMaterialOverrides(
			const Refl::RuntimeFieldDescriptor& field,
			const Refl::RuntimeTypeDescriptor& type,
			void* object,
			void* value,
			RuntimeInspectorContext& context) {
			if (field.Type != "MaterialOverrideSet") {
				ImGui::TextDisabled("Material overrides unavailable");
				return false;
			}
			const auto* sourceAttribute = Refl::FindRuntimeAttribute(field.Attributes, "Editor.SourceField");
			const auto* sourceField = sourceAttribute ? FindField(type, sourceAttribute->Value) : nullptr;
			if (!sourceField || sourceField->Type != "MaterialAssetRef") {
				ImGui::TextDisabled("Material reference unavailable");
				return false;
			}
			const auto* source = static_cast<const MaterialAssetRef*>(Refl::GetRuntimeFieldConst(*sourceField, object));
			if (!source) { ImGui::TextDisabled("Material reference unavailable"); return false; }
			const AssetGuid& materialGuid = source->Reference.Guid;
			auto& overrides = *static_cast<Rendering::MaterialOverrideSet*>(value);
			if (materialGuid.empty()) { ImGui::TextDisabled("Missing Material"); return false; }
			if (!context.ResolveMaterialDefinition) { ImGui::TextDisabled("Material definition unavailable"); return false; }
			Rendering::MaterialDefinition definition;
			AssetImportHealth importHealth;
			if (!context.ResolveMaterialDefinition(materialGuid, definition, importHealth).Succeeded()) {
				ImGui::TextDisabled(importHealth.State == AssetImportHealthState::Missing
					? "Material or shader artifact missing"
					: "Reimport required");
				return false;
			}
			if (importHealth.State == AssetImportHealthState::LastGoodWithFailure) {
				ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.20f, 1.0f), "Import failed; showing last-good parameters");
				if (!importHealth.Diagnostics.empty() && ImGui::IsItemHovered()) {
					ImGui::SetTooltip("%s", importHealth.Diagnostics.front().Message.c_str());
				}
			}
			bool changed = false;
			for (const auto& parameter : definition.GetParameters()) {
				ImGui::PushID(parameter.Name.c_str());
				ImGui::TextUnformatted(parameter.DisplayName.empty() ? parameter.Name.c_str() : parameter.DisplayName.c_str());
				ImGui::SameLine();
				if (parameter.Type == Rendering::ShaderValueType::Texture2D) {
					auto textureOverride = overrides.TextureParameters.find(parameter.Name);
					bool hasOverride = textureOverride != overrides.TextureParameters.end();
					if (hasOverride && ImGui::SmallButton("Reset")) {
						overrides.TextureParameters.erase(parameter.Name);
						changed = true;
						hasOverride = false;
					}
					const auto* defaultTexture = std::get_if<std::string>(&parameter.CurrentValue);
					AssetGuid guid = hasOverride ? textureOverride->second :
						(defaultTexture ? *defaultTexture : AssetGuid{});
					if (DrawAssetRefField(guid, AssetKind::Texture2D, context.TextureAssets)) {
						if (guid.empty()) overrides.TextureParameters.erase(parameter.Name);
						else overrides.TextureParameters[parameter.Name] = std::move(guid);
						changed = true;
					}
					ImGui::PopID();
					continue;
				}
				auto override = overrides.Parameters.find(parameter.Name);
				bool overridden = override != overrides.Parameters.end();
				if (overridden && ImGui::SmallButton("Reset")) {
					overrides.Parameters.erase(parameter.Name);
					changed = true; overridden = false;
				}
				auto value = overridden ? override->second : Rendering::MaterialParameterValue{};
				if (!overridden) {
					std::visit([&](const auto& current) {
						using T = std::decay_t<decltype(current)>;
						if constexpr (std::is_same_v<T, int> || std::is_same_v<T, float> || std::is_same_v<T, glm::vec2> || std::is_same_v<T, glm::vec3> || std::is_same_v<T, glm::vec4> || std::is_same_v<T, glm::mat4>) value = current;
					}, parameter.CurrentValue);
				}
				bool edited = false;
				const auto numericOptions = GetMaterialNumericEditorOptions(parameter);
				const float minimum = numericOptions.HasRange ? numericOptions.Minimum : 0.0f;
				const float maximum = numericOptions.HasRange ? numericOptions.Maximum : 0.0f;
				if (auto* scalar = std::get_if<int>(&value)) {
					const int minimumInt = static_cast<int>(std::ceil(minimum));
					const int maximumInt = static_cast<int>(std::floor(maximum));
					edited = ImGui::DragInt(
						"##Value",
						scalar,
						std::max(1.0f, numericOptions.Speed),
						minimumInt,
						maximumInt);
				}
				else if (auto* scalar = std::get_if<float>(&value)) edited = ImGui::DragFloat("##Value", scalar, numericOptions.Speed, minimum, maximum);
				else if (auto* vector = std::get_if<glm::vec2>(&value)) edited = ImGui::DragFloat2("##Value", &(*vector)[0], numericOptions.Speed, minimum, maximum);
				else if (auto* vector = std::get_if<glm::vec3>(&value)) edited = parameter.Editor == Rendering::ShaderEditorKind::Color ? ImGui::ColorEdit3("##Value", &(*vector)[0]) : ImGui::DragFloat3("##Value", &(*vector)[0], numericOptions.Speed, minimum, maximum);
				else if (auto* vector = std::get_if<glm::vec4>(&value)) edited = parameter.Editor == Rendering::ShaderEditorKind::Color ? ImGui::ColorEdit4("##Value", &(*vector)[0]) : ImGui::DragFloat4("##Value", &(*vector)[0], numericOptions.Speed, minimum, maximum);
				else ImGui::TextDisabled("Unsupported parameter type");
				if (edited) {
					overrides.Parameters[parameter.Name] = std::move(value);
					changed = true;
				}
				if (!parameter.Tooltip.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", parameter.Tooltip.c_str());
				ImGui::PopID();
			}
			return changed;
		}

		bool DrawRuntimeFieldEditorRow(
			const Refl::RuntimeFieldDescriptor& field,
			const Refl::RuntimeTypeDescriptor& type,
			void* object,
			const RuntimeFieldDrawerRegistry& drawers,
			RuntimeInspectorContext& context) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(FieldLabel(field));
			ImGui::TableSetColumnIndex(1);

			if (object == nullptr || field.GetMutable == nullptr) {
				ImGui::TextDisabled("Unavailable");
				return false;
			}
			if (Refl::IsRuntimeFieldReadOnly(field)) {
				ImGui::TextDisabled("Read only");
				return false;
			}
			const auto* namedDrawer = Refl::FindRuntimeAttribute(field.Attributes, "Inspector.Drawer");
			if (!namedDrawer && !Refl::IsRuntimeFieldEditable(field)) {
				ImGui::TextDisabled("Unavailable");
				return false;
			}
			void* value = Refl::GetRuntimeFieldMutable(field, object);
			if (value == nullptr) {
				ImGui::TextDisabled("Unavailable");
				return false;
			}

			ImGui::PushID(field.Name.data());
			ImGui::SetNextItemWidth(-1.0f);
			const RuntimeFieldDrawer* drawer = drawers.Resolve(field);
			bool changed = false;
			if (drawer) {
				changed = (*drawer)(field, type, object, value, context);
			}
			else if (namedDrawer) {
				ImGui::TextDisabled("Drawer unavailable: %.*s",
					static_cast<int>(namedDrawer->Value.size()), namedDrawer->Value.data());
			}
			else {
				ImGui::TextDisabled("Unsupported: %.*s", static_cast<int>(field.Type.size()), field.Type.data());
			}
			ImGui::PopID();
			return changed;
		}
	}

	bool SetRuntimeMaterialReference(
		const Refl::RuntimeFieldDescriptor& field,
		const Refl::RuntimeTypeDescriptor& type,
		void* object,
		const AssetGuid& nextGuid,
		RuntimeInspectorContext& context) {
		if (field.Type != "MaterialAssetRef" || Refl::IsRuntimeFieldReadOnly(field)) return false;
		auto* guid = GetAssetRefGuid(field, Refl::GetRuntimeFieldMutable(field, object));
		const auto* reconcileAttribute = Refl::FindRuntimeAttribute(field.Attributes, "Editor.ReconcileField");
		const auto* reconcileField = reconcileAttribute ? FindField(type, reconcileAttribute->Value) : nullptr;
		if (!guid || !reconcileField || reconcileField->Type != "MaterialOverrideSet" ||
			Refl::IsRuntimeFieldReadOnly(*reconcileField)) return false;
		auto* overrides = static_cast<Rendering::MaterialOverrideSet*>(
			Refl::GetRuntimeFieldMutable(*reconcileField, object));
		if (!overrides || *guid == nextGuid) return false;
		*guid = nextGuid;

		if (context.ResolveMaterialDefinition && !guid->empty()) {
			Rendering::MaterialDefinition definition;
			AssetImportHealth health;
			if (context.ResolveMaterialDefinition(*guid, definition, health).Succeeded() &&
				Rendering::ReconcileMaterialOverrides(*overrides, definition) && context.DeferredEvents) {
				auto warning = ResultEnvelope::Success(
					"editor.material_overrides.reconciled", *guid, "Incompatible material overrides were removed");
				warning.AddDetail({DiagnosticSeverity::Warning, "editor.material_overrides.removed",
					"The selected material does not define one or more previous overrides", *guid});
				context.DeferredEvents->push_back(std::move(warning));
			}
		}
		return true;
	}

	RuntimeFieldDrawerRegistry::RuntimeFieldDrawerRegistry() {
		RegisterKind(Refl::RuntimeFieldValueKind::Bool,
			[](const auto&, const auto&, void*, void* value, auto&) {
				return ImGui::Checkbox("##Value", static_cast<bool*>(value));
			});
		RegisterKind(Refl::RuntimeFieldValueKind::SignedInteger,
			[](const auto& field, const auto&, void*, void* value, auto&) {
				return ImGui::DragScalar("##Value", ScalarTypeForSignedField(field), value, 1.0f);
			});
		RegisterKind(Refl::RuntimeFieldValueKind::UnsignedInteger,
			[](const auto& field, const auto&, void*, void* value, auto&) {
				return ImGui::DragScalar("##Value", ScalarTypeForUnsignedField(field), value, 1.0f);
			});
		RegisterKind(Refl::RuntimeFieldValueKind::Float,
			[](const auto& field, const auto&, void*, void* value, auto&) {
				const auto range = FloatEditorRange(field);
				return ImGui::DragFloat("##Value", static_cast<float*>(value), 0.1f,
					range ? range->first : 0.0f, range ? range->second : 0.0f, "%.3f",
					range ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None);
			});
		RegisterKind(Refl::RuntimeFieldValueKind::Double,
			[](const auto&, const auto&, void*, void* value, auto&) {
				return ImGui::DragScalar("##Value", ImGuiDataType_Double, value, 0.1f);
			});
		RegisterKind(Refl::RuntimeFieldValueKind::String,
			[](const auto&, const auto&, void*, void* value, auto&) {
				auto& text = *static_cast<std::string*>(value);
				return ImGui::InputText("##Value", text.data(), text.capacity() + 1,
					ImGuiInputTextFlags_CallbackResize, &ResizeStringInputCallback, &text);
			});
		RegisterKind(Refl::RuntimeFieldValueKind::Float2,
			[](const auto&, const auto&, void*, void* value, auto&) {
				return ImGui::DragFloat2("##Value", static_cast<float*>(value), 0.1f);
			});
		RegisterKind(Refl::RuntimeFieldValueKind::Float3,
			[](const auto&, const auto&, void*, void* value, auto&) {
				return ImGui::DragFloat3("##Value", static_cast<float*>(value), 0.1f);
			});
		RegisterKind(Refl::RuntimeFieldValueKind::Float4,
			[](const auto&, const auto&, void*, void* value, auto&) {
				return ImGui::DragFloat4("##Value", static_cast<float*>(value), 0.1f);
			});
		RegisterKind(Refl::RuntimeFieldValueKind::Enum,
			[](const auto& field, const auto&, void* object, void*, auto&) {
				return DrawRuntimeEnumField(field, object);
			});
		RegisterKind(Refl::RuntimeFieldValueKind::AssetRef,
			[](const auto& field, const auto&, void*, void* value, auto& context) {
				return DrawRuntimeAssetRefField(field, value, context);
			});
		RegisterNamed("MaterialReference", DrawMaterialReference);
		RegisterNamed("MaterialOverrides", DrawMaterialOverrides);
	}

	void RuntimeFieldDrawerRegistry::RegisterNamed(std::string_view name, RuntimeFieldDrawer drawer) {
		if (!name.empty() && drawer) m_Named[std::string(name)] = std::move(drawer);
	}

	void RuntimeFieldDrawerRegistry::RegisterKind(Refl::RuntimeFieldValueKind kind, RuntimeFieldDrawer drawer) {
		if (drawer) m_ByKind[kind] = std::move(drawer);
	}

	const RuntimeFieldDrawer* RuntimeFieldDrawerRegistry::Resolve(const Refl::RuntimeFieldDescriptor& field) const {
		if (const auto* attribute = Refl::FindRuntimeAttribute(field.Attributes, "Inspector.Drawer")) {
			const auto found = m_Named.find(attribute->Value);
			return found == m_Named.end() ? nullptr : &found->second;
		}
		const auto found = m_ByKind.find(Refl::GetRuntimeFieldValueKind(field));
		return found == m_ByKind.end() ? nullptr : &found->second;
	}

	bool IsRuntimeFieldEditable(const Refl::RuntimeFieldDescriptor& field) {
		return Refl::IsRuntimeFieldEditable(field);
	}

	std::string GetRuntimeComponentDisplayName(const Refl::RuntimeTypeDescriptor& type) {
		if (!type.DisplayName.empty()) {
			return std::string(type.DisplayName);
		}
		if (!type.Name.empty()) {
			return std::string(type.Name);
		}
		return std::string(type.QualifiedName);
	}

	MaterialNumericEditorOptions GetMaterialNumericEditorOptions(
		const Rendering::MaterialParameterDefinition& parameter) {
		MaterialNumericEditorOptions options;
		if (parameter.Step > 0.0f) {
			options.Speed = parameter.Step;
		}
		if (parameter.Range.size() == 2 && parameter.Range[0] <= parameter.Range[1]) {
			options.Minimum = parameter.Range[0];
			options.Maximum = parameter.Range[1];
			options.HasRange = true;
		}
		return options;
	}

	bool DrawRuntimeFields(
		const Refl::RuntimeTypeDescriptor& type,
		void* object,
		const RuntimeFieldDrawerRegistry& drawers,
		RuntimeInspectorContext context) {
		if (object == nullptr) return false;

		if (!BeginRuntimeFieldTable("##RuntimeFields")) {
			return false;
		}

		bool changed = false;
		std::string_view currentCategory;
		for (const Refl::RuntimeFieldDescriptor& field : type.Fields) {
			if (!field.Category.empty() && field.Category != currentCategory) {
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextDisabled("%.*s", static_cast<int>(field.Category.size()), field.Category.data());
			}
			currentCategory = field.Category;
			changed |= DrawRuntimeFieldEditorRow(field, type, object, drawers, context);
		}
		ImGui::EndTable();
		return changed;
	}
}
