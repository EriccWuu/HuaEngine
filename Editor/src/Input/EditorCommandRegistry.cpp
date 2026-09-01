#include "EditorCommandRegistry.h"

namespace HE::Editor {
	ResultEnvelope EditorCommandRegistry::Register(EditorCommandDescriptor descriptor) {
		if (descriptor.Id.empty() || !descriptor.Execute) {
			return ResultEnvelope::Failure("editor.command.register", descriptor.Id, "Command registration is incomplete");
		}
		if (m_Commands.contains(descriptor.Id)) {
			return ResultEnvelope::Failure("editor.command.register", descriptor.Id, "Command is already registered");
		}
		const auto id = descriptor.Id;
		m_Commands.emplace(id, std::move(descriptor));
		return ResultEnvelope::Success("editor.command.register", id, "Command registered");
	}

	void EditorCommandRegistry::Clear() {
		m_Commands.clear();
	}

	const EditorCommandDescriptor* EditorCommandRegistry::Find(std::string_view commandId) const {
		const auto iterator = m_Commands.find(std::string(commandId));
		return iterator == m_Commands.end() ? nullptr : &iterator->second;
	}

	bool EditorCommandRegistry::CanExecute(std::string_view commandId) const {
		const auto* command = Find(commandId);
		return command && (!command->CanExecute || command->CanExecute());
	}

	ResultEnvelope EditorCommandRegistry::Execute(std::string_view commandId) const {
		const auto* command = Find(commandId);
		if (!command) return ResultEnvelope::Failure("editor.command.execute", std::string(commandId), "Command is not registered");
		if (command->CanExecute && !command->CanExecute()) {
			return ResultEnvelope::Failure("editor.command.execute", command->Id, "Command is currently disabled");
		}
		command->Execute();
		return ResultEnvelope::Success("editor.command.execute", command->Id, "Command executed");
	}
}
