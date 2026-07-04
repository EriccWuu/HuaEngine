#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "HuaEngine/Reflection/Reflection.h"

namespace HE::Editor {
	enum class RuntimeFieldEditKind {
		Unsupported,
		Bool,
		Int,
		UInt,
		Float,
		Double,
		String,
		Float2,
		Float3,
		Float4,
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

	[[nodiscard]] RuntimeFieldEditKind GetRuntimeFieldEditKind(const Refl::RuntimeFieldDescriptor& field);
	[[nodiscard]] bool IsRuntimeFieldEditable(const Refl::RuntimeFieldDescriptor& field);
	[[nodiscard]] std::string GetRuntimeComponentDisplayName(const Refl::RuntimeTypeDescriptor& type);

	bool DrawRuntimeFieldEditor(const Refl::RuntimeFieldDescriptor& field, void* component);
	bool DrawRuntimeComponentInspector(
		const Refl::RuntimeTypeDescriptor& type,
		void* component,
		const RuntimeComponentEditorOverrideRegistry& overrides);
}
