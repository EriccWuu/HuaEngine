#include "EditorInputContextService.h"

#include <algorithm>

namespace HE::Editor {
	void EditorInputContextService::BeginFrame() {
		m_Active.clear();
		Activate("Global", 0, true, true, false);
	}

	void EditorInputContextService::Activate(std::string contextId, int priority, bool keyboard, bool pointer, bool blocksLower) {
		const auto iterator = std::ranges::find_if(m_Active, [&](const auto& context) { return context.Id == contextId; });
		if (iterator != m_Active.end()) {
			iterator->Priority = std::max(iterator->Priority, priority);
			iterator->Keyboard = iterator->Keyboard || keyboard;
			iterator->Pointer = iterator->Pointer || pointer;
			iterator->BlocksLower = iterator->BlocksLower || blocksLower;
			return;
		}
		m_Active.push_back({ std::move(contextId), priority, keyboard, pointer, blocksLower });
	}

	std::optional<int> EditorInputContextService::GetPriority(std::string_view contextId, InputDeviceType device) const {
		const auto iterator = std::ranges::find_if(m_Active, [&](const auto& context) { return context.Id == contextId; });
		if (iterator == m_Active.end()) return std::nullopt;
		const bool acceptsDevice = device == InputDeviceType::Keyboard ? iterator->Keyboard : iterator->Pointer;
		return acceptsDevice ? std::optional(iterator->Priority) : std::nullopt;
	}

	bool EditorInputContextService::IsBlocked(int priority, InputDeviceType device) const {
		return std::ranges::any_of(m_Active, [&](const auto& context) {
			const bool acceptsDevice = device == InputDeviceType::Keyboard ? context.Keyboard : context.Pointer;
			return acceptsDevice && context.BlocksLower && context.Priority > priority;
		});
	}
}
