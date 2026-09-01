#include "enginepch.h"
#include "HuaEngine/Input/InputSnapshot.h"

namespace HE {
	bool InputSnapshot::IsDown(InputControl control) const {
		return m_Down.contains(control);
	}

	bool InputSnapshot::WasPressed(InputControl control) const {
		return m_Pressed.contains(control);
	}

	bool InputSnapshot::WasReleased(InputControl control) const {
		return m_Released.contains(control);
	}

	bool InputSnapshot::WasRepeated(InputControl control) const {
		return m_Repeated.contains(control);
	}

	bool InputSnapshot::WasDoublePressed(InputControl control) const {
		return m_DoublePressed.contains(control);
	}
}
