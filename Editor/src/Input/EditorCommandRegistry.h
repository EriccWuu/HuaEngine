#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "HuaEngine/Core/ResultEnvelope.h"

namespace HE::Editor {
	struct EditorCommandDescriptor {
		std::string Id;
		std::string DisplayName;
		std::string Category;
		std::function<bool()> CanExecute;
		std::function<void()> Execute;
	};

	class EditorCommandRegistry {
	public:
		ResultEnvelope Register(EditorCommandDescriptor descriptor);
		void Clear();

		[[nodiscard]] const EditorCommandDescriptor* Find(std::string_view commandId) const;
		[[nodiscard]] bool CanExecute(std::string_view commandId) const;
		ResultEnvelope Execute(std::string_view commandId) const;

	private:
		std::unordered_map<std::string, EditorCommandDescriptor> m_Commands;
	};
}
