#include "EditorInputResolver.h"

#include <algorithm>

namespace HE::Editor {
	namespace {
		bool Matches(const InputGesture& gesture, const InputSnapshot& snapshot) {
			if (gesture.ExactModifiers) {
				if (snapshot.GetModifiers() != gesture.Modifiers) return false;
			} else if ((snapshot.GetModifiers() & gesture.Modifiers) != gesture.Modifiers) {
				return false;
			}
			switch (gesture.Trigger) {
			case InputTrigger::Pressed: return snapshot.WasPressed(gesture.Primary);
			case InputTrigger::Released: return snapshot.WasReleased(gesture.Primary);
			case InputTrigger::Repeated: return snapshot.WasRepeated(gesture.Primary);
			case InputTrigger::Held: return snapshot.IsDown(gesture.Primary);
			case InputTrigger::DoublePressed: return snapshot.WasDoublePressed(gesture.Primary);
			}
			return false;
		}

		struct CommandCandidate {
			const EditorCommandBinding* Binding = nullptr;
			int ContextPriority = 0;
		};
	}

	ResultEnvelope EditorInputResolver::Resolve(
		const InputSnapshot& snapshot,
		const EditorCommandRegistry& commands,
		const EditorInputBindingRegistry& bindings,
		const EditorInputContextService& contexts) {
		m_ActionValues.clear();
		const auto effectiveBindings = bindings.GetEffectiveCommandBindings();
		std::vector<CommandCandidate> candidates;
		for (const auto& binding : effectiveBindings) {
			if (!Matches(binding.Gesture, snapshot)) continue;
			const auto contextPriority = contexts.GetPriority(binding.ContextId, binding.Gesture.Primary.Device);
			if (!contextPriority || contexts.IsBlocked(*contextPriority, binding.Gesture.Primary.Device)) continue;
			if (!commands.CanExecute(binding.CommandId)) continue;
			candidates.push_back({ &binding, *contextPriority });
		}

		std::ranges::sort(candidates, [](const auto& lhs, const auto& rhs) {
			if (lhs.ContextPriority != rhs.ContextPriority) return lhs.ContextPriority > rhs.ContextPriority;
			return lhs.Binding->Priority > rhs.Binding->Priority;
		});

		while (!candidates.empty()) {
			const auto gesture = candidates.front().Binding->Gesture;
			std::vector<CommandCandidate> matching;
			std::ranges::copy_if(candidates, std::back_inserter(matching), [&](const auto& candidate) {
				return candidate.Binding->Gesture == gesture;
			});
			const auto topContext = matching.front().ContextPriority;
			const auto topBindingPriority = matching.front().Binding->Priority;
			const auto conflictCount = std::ranges::count_if(matching, [&](const auto& candidate) {
				return candidate.ContextPriority == topContext && candidate.Binding->Priority == topBindingPriority;
			});
			if (conflictCount > 1) {
				return ResultEnvelope::Failure("editor.input.resolve", matching.front().Binding->ContextId, "Multiple enabled commands use the same highest-priority gesture");
			}
			commands.Execute(matching.front().Binding->CommandId);
			if (matching.front().Binding->Consume) {
				std::erase_if(candidates, [&](const auto& candidate) { return candidate.Binding->Gesture == gesture; });
			} else {
				candidates.erase(candidates.begin());
			}
		}

		for (const auto& binding : bindings.GetActionBindings()) {
			if (!Matches(binding.Gesture, snapshot)) continue;
			const auto contextPriority = contexts.GetPriority(binding.ContextId, binding.Gesture.Primary.Device);
			if (!contextPriority || contexts.IsBlocked(*contextPriority, binding.Gesture.Primary.Device)) continue;
			m_ActionValues[binding.ActionId] += binding.Scale;
		}
		return ResultEnvelope::Success("editor.input.resolve", "frame", "Input frame resolved");
	}

	float EditorInputResolver::GetActionValue(std::string_view actionId) const {
		const auto iterator = m_ActionValues.find(std::string(actionId));
		return iterator == m_ActionValues.end() ? 0.0f : iterator->second;
	}

	bool EditorInputResolver::WasActionTriggered(std::string_view actionId) const {
		return GetActionValue(actionId) != 0.0f;
	}
}
