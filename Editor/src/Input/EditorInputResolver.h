#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "EditorCommandRegistry.h"
#include "EditorInputBindingRegistry.h"
#include "EditorInputContextService.h"
#include "HuaEngine/Input/InputSnapshot.h"

namespace HE::Editor {
	class EditorInputResolver {
	public:
		ResultEnvelope Resolve(
			const InputSnapshot& snapshot,
			const EditorCommandRegistry& commands,
			const EditorInputBindingRegistry& bindings,
			const EditorInputContextService& contexts);

		[[nodiscard]] float GetActionValue(std::string_view actionId) const;
		[[nodiscard]] bool WasActionTriggered(std::string_view actionId) const;
		void Reset();

	private:
		std::unordered_map<std::string, float> m_ActionValues;
		std::unordered_set<InputControl, InputControlHash> m_SuppressedControls;
	};
}
