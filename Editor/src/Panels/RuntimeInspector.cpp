#include "RuntimeInspector.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <utility>

#include "glm/glm.hpp"
#include "imgui.h"
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

		bool DrawRuntimeEnumField(const Refl::RuntimeFieldDescriptor& field, void* component, const char* label) {
			if (field.EnumType == nullptr) {
				ImGui::TextDisabled("%s: enum metadata unavailable", label);
				return false;
			}

			int64_t currentValue = 0;
			if (!Refl::GetRuntimeEnumFieldValue(field, component, currentValue)) {
				ImGui::TextDisabled("%s: enum value unavailable", label);
				return false;
			}

			const Refl::RuntimeEnumValueDescriptor* current =
				Refl::FindRuntimeEnumValueByValue(*field.EnumType, currentValue);
			const char* preview = current != nullptr
				? (current->DisplayName.empty() ? current->Name.data() : current->DisplayName.data())
				: "<unknown>";

			bool changed = false;
			if (ImGui::BeginCombo(label, preview)) {
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

		bool DrawMeshAssetRefField(
			AssetGuid& guid,
			const char* label,
			std::span<const AssetPickerOption> options) {
			const AssetPickerPreview preview = GetAssetPickerPreview(options, guid);
			bool changed = false;
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::BeginCombo(label, preview.DisplayName.c_str(), ImGuiComboFlags_HeightLarge)) {
				static std::array<char, 128> searchBuffer{};
				if (ImGui::IsWindowAppearing()) {
					searchBuffer.fill('\0');
				}

				ImGui::SetNextItemWidth(-1.0f);
				ImGui::InputTextWithHint("##MeshSearch", "Search meshes...", searchBuffer.data(), searchBuffer.size());
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
					ImGui::TextDisabled("No matching mesh assets");
				}
				ImGui::EndCombo();
			}

			if (preview.Missing && ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Mesh asset is not present in the current project: %s", guid.c_str());
			}
			return changed;
		}

		bool DrawRuntimeAssetRefField(
			const Refl::RuntimeFieldDescriptor& field,
			void* value,
			const char* label,
			RuntimeInspectorContext context) {
			AssetGuid* guid = GetAssetRefGuid(field, value);
			if (guid == nullptr) {
				ImGui::TextDisabled("%s: unsupported asset ref %.*s", label, static_cast<int>(field.Type.size()), field.Type.data());
				return false;
			}
			if (field.Type == "MeshAssetRef") {
				return DrawMeshAssetRefField(*guid, label, context.MeshAssets);
			}

			std::array<char, 256> editedGuid{};
			const size_t copyLength = std::min(guid->size(), editedGuid.size() - 1);
			std::memcpy(editedGuid.data(), guid->data(), copyLength);
			const bool changed = ImGui::InputText(
				label,
				editedGuid.data(),
				editedGuid.size());
			if (changed) {
				*guid = editedGuid.data();
			}
			return changed;
		}
	}

	void RuntimeComponentEditorOverrideRegistry::RegisterOverride(
		std::string_view qualifiedName,
		RuntimeComponentEditorOverride editor) {
		if (qualifiedName.empty() || !editor) {
			return;
		}
		m_Overrides[std::string(qualifiedName)] = std::move(editor);
	}

	const RuntimeComponentEditorOverride* RuntimeComponentEditorOverrideRegistry::FindOverride(
		std::string_view qualifiedName) const {
		const auto iterator = m_Overrides.find(std::string(qualifiedName));
		return iterator != m_Overrides.end() ? &iterator->second : nullptr;
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

	bool DrawRuntimeFieldEditor(
		const Refl::RuntimeFieldDescriptor& field,
		void* component,
		RuntimeInspectorContext context) {
		if (component == nullptr || field.GetMutable == nullptr) {
			ImGui::TextDisabled("%s: unavailable", FieldLabel(field));
			return false;
		}

		void* value = field.GetMutable(component);
		if (value == nullptr) {
			ImGui::TextDisabled("%s: unavailable", FieldLabel(field));
			return false;
		}

		ImGui::PushID(field.Name.data());
		bool changed = false;
		const char* label = FieldLabel(field);
		switch (Refl::GetRuntimeFieldValueKind(field)) {
			case Refl::RuntimeFieldValueKind::Bool:
				changed = ImGui::Checkbox(label, static_cast<bool*>(value));
				break;
			case Refl::RuntimeFieldValueKind::SignedInteger:
				changed = ImGui::DragScalar(label, ScalarTypeForSignedField(field), value, 1.0f);
				break;
			case Refl::RuntimeFieldValueKind::UnsignedInteger:
				changed = ImGui::DragScalar(label, ScalarTypeForUnsignedField(field), value, 1.0f);
				break;
			case Refl::RuntimeFieldValueKind::Float:
				changed = ImGui::DragFloat(label, static_cast<float*>(value), 0.1f);
				break;
			case Refl::RuntimeFieldValueKind::Double:
				changed = ImGui::DragScalar(label, ImGuiDataType_Double, value, 0.1f);
				break;
			case Refl::RuntimeFieldValueKind::String: {
				auto& text = *static_cast<std::string*>(value);
				changed = ImGui::InputText(
					label,
					text.data(),
					text.capacity() + 1,
					ImGuiInputTextFlags_CallbackResize,
					&ResizeStringInputCallback,
					&text);
				break;
			}
			case Refl::RuntimeFieldValueKind::Float2:
				changed = ImGui::DragFloat2(label, static_cast<float*>(value), 0.1f);
				break;
			case Refl::RuntimeFieldValueKind::Float3:
				changed = ImGui::DragFloat3(label, static_cast<float*>(value), 0.1f);
				break;
			case Refl::RuntimeFieldValueKind::Float4:
				changed = ImGui::DragFloat4(label, static_cast<float*>(value), 0.1f);
				break;
			case Refl::RuntimeFieldValueKind::Enum:
				changed = DrawRuntimeEnumField(field, component, label);
				break;
			case Refl::RuntimeFieldValueKind::AssetRef:
				changed = DrawRuntimeAssetRefField(field, value, label, context);
				break;
			case Refl::RuntimeFieldValueKind::Unsupported:
			case Refl::RuntimeFieldValueKind::Object:
			default:
				ImGui::TextDisabled("%s: unsupported %.*s", label, static_cast<int>(field.Type.size()), field.Type.data());
				break;
		}
		ImGui::PopID();
		return changed;
	}

	bool DrawRuntimeComponentInspector(
		const Refl::RuntimeTypeDescriptor& type,
		void* component,
		const RuntimeComponentEditorOverrideRegistry& overrides,
		RuntimeInspectorContext context) {
		if (component == nullptr) {
			return false;
		}

		if (const RuntimeComponentEditorOverride* editor = overrides.FindOverride(type.QualifiedName)) {
			return (*editor)(type, component);
		}

		bool changed = false;
		for (const Refl::RuntimeFieldDescriptor& field : type.Fields) {
			changed |= DrawRuntimeFieldEditor(field, component, context);
		}
		return changed;
	}
}
