#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "HuaEngine/Input/InputTypes.h"

namespace HE::Editor {
	struct EditorInputContextState {
		std::string Id;
		int Priority = 0;
		bool Keyboard = false;
		bool Pointer = false;
		bool BlocksLower = false;
	};

	class EditorInputContextService {
	public:
		void BeginFrame();
		void Activate(std::string contextId, int priority, bool keyboard, bool pointer, bool blocksLower);

		[[nodiscard]] std::optional<int> GetPriority(std::string_view contextId, InputDeviceType device) const;
		[[nodiscard]] bool IsBlocked(int priority, InputDeviceType device) const;

	private:
		std::vector<EditorInputContextState> m_Active;
	};
}
