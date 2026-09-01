#pragma once

#include "EditorCommandRegistry.h"
#include "EditorInputBindingRegistry.h"
#include "EditorInputContextService.h"
#include "EditorInputResolver.h"

namespace HE::Editor {
	class EditorInputService {
	public:
		[[nodiscard]] EditorCommandRegistry& Commands() { return m_Commands; }
		[[nodiscard]] const EditorCommandRegistry& Commands() const { return m_Commands; }
		[[nodiscard]] EditorInputBindingRegistry& Bindings() { return m_Bindings; }
		[[nodiscard]] const EditorInputBindingRegistry& Bindings() const { return m_Bindings; }
		[[nodiscard]] EditorInputContextService& Contexts() { return m_Contexts; }
		[[nodiscard]] const EditorInputContextService& Contexts() const { return m_Contexts; }

		ResultEnvelope Resolve(const InputSnapshot& snapshot) { return m_Resolver.Resolve(snapshot, m_Commands, m_Bindings, m_Contexts); }
		[[nodiscard]] float GetActionValue(std::string_view actionId) const { return m_Resolver.GetActionValue(actionId); }
		[[nodiscard]] bool WasActionTriggered(std::string_view actionId) const { return m_Resolver.WasActionTriggered(actionId); }
		void Reset();

	private:
		EditorCommandRegistry m_Commands;
		EditorInputBindingRegistry m_Bindings;
		EditorInputContextService m_Contexts;
		EditorInputResolver m_Resolver;
	};
}
