#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Input/InputTypes.h"

namespace HE::Editor {
	enum class EditorActionValueSource : uint8_t {
		Digital,
		PointerDeltaX,
		PointerDeltaY,
		ScrollX,
		ScrollY
	};

	struct EditorCommandBinding {
		std::string Id;
		std::string CommandId;
		std::string ContextId;
		InputGesture Gesture;
		int Priority = 0;
		bool Consume = true;
	};

	struct EditorActionBinding {
		std::string Id;
		std::string ActionId;
		std::string ContextId;
		InputGesture Gesture;
		float Scale = 1.0f;
		int Priority = 0;
		EditorActionValueSource ValueSource = EditorActionValueSource::Digital;
	};

	struct EditorInputBindingOverride {
		std::string CommandId;
		std::string ContextId;
		InputGesture Gesture;

		[[nodiscard]] bool operator==(const EditorInputBindingOverride&) const = default;
	};

	class EditorInputBindingRegistry {
	public:
		ResultEnvelope RegisterDefaultCommand(EditorCommandBinding binding);
		ResultEnvelope RegisterDefaultAction(EditorActionBinding binding);
		ResultEnvelope SetOverrides(std::vector<EditorInputBindingOverride> overrides);
		ResultEnvelope SetOverride(EditorInputBindingOverride overrideBinding);
		void ResetOverride(std::string_view commandId);
		void Clear();

		[[nodiscard]] std::vector<EditorCommandBinding> GetEffectiveCommandBindings() const;
		[[nodiscard]] const std::vector<EditorActionBinding>& GetActionBindings() const { return m_Actions; }
		[[nodiscard]] const std::vector<EditorInputBindingOverride>& GetOverrides() const { return m_Overrides; }
		[[nodiscard]] std::string GetDisplayText(std::string_view commandId) const;
		[[nodiscard]] std::vector<std::string> FindConflicts(std::string_view contextId, const InputGesture& gesture) const;

	private:
		std::vector<EditorCommandBinding> m_Commands;
		std::vector<EditorActionBinding> m_Actions;
		std::vector<EditorInputBindingOverride> m_Overrides;
	};
}
