#include "RuntimeInspector.h"

#include <algorithm>
#include <initializer_list>
#include <utility>

#include "glm/glm.hpp"
#include "imgui.h"

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

		bool IsAnyOf(std::string_view value, std::initializer_list<std::string_view> candidates) {
			for (std::string_view candidate : candidates) {
				if (value == candidate) {
					return true;
				}
			}
			return false;
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

	RuntimeFieldEditKind GetRuntimeFieldEditKind(const Refl::RuntimeFieldDescriptor& field) {
		if (!Refl::HasRuntimeFieldFlag(field.Flags, Refl::RuntimeFieldFlags::Serializable) ||
			field.GetMutable == nullptr) {
			return RuntimeFieldEditKind::Unsupported;
		}

		if (field.Type == "bool") {
			return RuntimeFieldEditKind::Bool;
		}
		if (IsAnyOf(field.Type, { "int", "int8_t", "int16_t", "int32_t", "int64_t" })) {
			return RuntimeFieldEditKind::Int;
		}
		if (IsAnyOf(field.Type, { "unsigned int", "uint8_t", "uint16_t", "uint32_t", "uint64_t" })) {
			return RuntimeFieldEditKind::UInt;
		}
		if (field.Type == "float") {
			return RuntimeFieldEditKind::Float;
		}
		if (field.Type == "double") {
			return RuntimeFieldEditKind::Double;
		}
		if (field.Type == "std::string") {
			return RuntimeFieldEditKind::String;
		}
		if (field.Type == "glm::vec2") {
			return RuntimeFieldEditKind::Float2;
		}
		if (field.Type == "glm::vec3") {
			return RuntimeFieldEditKind::Float3;
		}
		if (field.Type == "glm::vec4") {
			return RuntimeFieldEditKind::Float4;
		}

		return RuntimeFieldEditKind::Unsupported;
	}

	bool IsRuntimeFieldEditable(const Refl::RuntimeFieldDescriptor& field) {
		return GetRuntimeFieldEditKind(field) != RuntimeFieldEditKind::Unsupported;
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

	bool DrawRuntimeFieldEditor(const Refl::RuntimeFieldDescriptor& field, void* component) {
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
		switch (GetRuntimeFieldEditKind(field)) {
			case RuntimeFieldEditKind::Bool:
				changed = ImGui::Checkbox(label, static_cast<bool*>(value));
				break;
			case RuntimeFieldEditKind::Int:
				changed = ImGui::DragScalar(label, ScalarTypeForSignedField(field), value, 1.0f);
				break;
			case RuntimeFieldEditKind::UInt:
				changed = ImGui::DragScalar(label, ScalarTypeForUnsignedField(field), value, 1.0f);
				break;
			case RuntimeFieldEditKind::Float:
				changed = ImGui::DragFloat(label, static_cast<float*>(value), 0.1f);
				break;
			case RuntimeFieldEditKind::Double:
				changed = ImGui::DragScalar(label, ImGuiDataType_Double, value, 0.1f);
				break;
			case RuntimeFieldEditKind::String: {
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
			case RuntimeFieldEditKind::Float2:
				changed = ImGui::DragFloat2(label, static_cast<float*>(value), 0.1f);
				break;
			case RuntimeFieldEditKind::Float3:
				changed = ImGui::DragFloat3(label, static_cast<float*>(value), 0.1f);
				break;
			case RuntimeFieldEditKind::Float4:
				changed = ImGui::DragFloat4(label, static_cast<float*>(value), 0.1f);
				break;
			case RuntimeFieldEditKind::Unsupported:
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
		const RuntimeComponentEditorOverrideRegistry& overrides) {
		if (component == nullptr) {
			return false;
		}

		if (const RuntimeComponentEditorOverride* editor = overrides.FindOverride(type.QualifiedName)) {
			return (*editor)(type, component);
		}

		bool changed = false;
		for (const Refl::RuntimeFieldDescriptor& field : type.Fields) {
			changed |= DrawRuntimeFieldEditor(field, component);
		}
		return changed;
	}
}
