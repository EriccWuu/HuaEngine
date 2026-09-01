#include "EditorInputService.h"

namespace HE::Editor {
	void EditorInputService::Reset() {
		m_Commands.Clear();
		m_Bindings.Clear();
		m_Contexts.BeginFrame();
	}
}
