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
			RuntimeInspectorContext context) {
			AssetGuid* guid = GetAssetRefGuid(field, value);
			if (guid == nullptr) {
				ImGui::TextDisabled("Unsupported asset ref: %.*s", static_cast<int>(field.Type.size()), field.Type.data());
				return false;
			}
			if (field.Type == "MeshAssetRef" || field.Type == "MaterialAssetRef") {
				const AssetKind kind = field.Type == "MeshAssetRef" ? AssetKind::Mesh : AssetKind::Material;
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

		bool DrawRuntimeFieldEditorRow(
			const Refl::RuntimeFieldDescriptor& field,
			void* component,
			RuntimeInspectorContext context) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(FieldLabel(field));
			ImGui::TableSetColumnIndex(1);

			if (component == nullptr || field.GetMutable == nullptr) {
				ImGui::TextDisabled("Unavailable");
				return false;
			}

			void* value = field.GetMutable(component);
			if (value == nullptr) {
				ImGui::TextDisabled("Unavailable");
				return false;
			}

			ImGui::PushID(field.Name.data());
			ImGui::SetNextItemWidth(-1.0f);
			bool changed = false;
			switch (Refl::GetRuntimeFieldValueKind(field)) {
			case Refl::RuntimeFieldValueKind::Bool:
				changed = ImGui::Checkbox("##Value", static_cast<bool*>(value));
				break;
			case Refl::RuntimeFieldValueKind::SignedInteger:
				changed = ImGui::DragScalar("##Value", ScalarTypeForSignedField(field), value, 1.0f);
				break;
			case Refl::RuntimeFieldValueKind::UnsignedInteger:
				changed = ImGui::DragScalar("##Value", ScalarTypeForUnsignedField(field), value, 1.0f);
				break;
			case Refl::RuntimeFieldValueKind::Float:
				changed = ImGui::DragFloat("##Value", static_cast<float*>(value), 0.1f);
				break;
			case Refl::RuntimeFieldValueKind::Double:
				changed = ImGui::DragScalar("##Value", ImGuiDataType_Double, value, 0.1f);
				break;
			case Refl::RuntimeFieldValueKind::String: {
				auto& text = *static_cast<std::string*>(value);
				changed = ImGui::InputText(
					"##Value",
					text.data(),
					text.capacity() + 1,
					ImGuiInputTextFlags_CallbackResize,
					&ResizeStringInputCallback,
					&text);
				break;
			}
			case Refl::RuntimeFieldValueKind::Float2:
				changed = ImGui::DragFloat2("##Value", static_cast<float*>(value), 0.1f);
				break;
			case Refl::RuntimeFieldValueKind::Float3:
				changed = ImGui::DragFloat3("##Value", static_cast<float*>(value), 0.1f);
				break;
			case Refl::RuntimeFieldValueKind::Float4:
				changed = ImGui::DragFloat4("##Value", static_cast<float*>(value), 0.1f);
				break;
			case Refl::RuntimeFieldValueKind::Enum:
				changed = DrawRuntimeEnumField(field, component);
				break;
			case Refl::RuntimeFieldValueKind::AssetRef:
				changed = DrawRuntimeAssetRefField(field, value, context);
				break;
			case Refl::RuntimeFieldValueKind::Unsupported:
			case Refl::RuntimeFieldValueKind::Object:
			default:
				ImGui::TextDisabled("Unsupported: %.*s", static_cast<int>(field.Type.size()), field.Type.data());
				break;
			}
			ImGui::PopID();
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
		ImGui::PushID(field.Name.data());
		if (!BeginRuntimeFieldTable("##RuntimeField")) {
			ImGui::PopID();
			return false;
		}
		const bool changed = DrawRuntimeFieldEditorRow(field, component, context);
		ImGui::EndTable();
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

		if (!BeginRuntimeFieldTable("##RuntimeFields")) {
			return false;
		}

		bool changed = false;
		for (const Refl::RuntimeFieldDescriptor& field : type.Fields) {
			changed |= DrawRuntimeFieldEditorRow(field, component, context);
		}
		ImGui::EndTable();
		return changed;
	}
}
