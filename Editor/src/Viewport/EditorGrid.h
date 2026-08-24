#pragma once

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"

namespace HE::Editor {
	struct EditorGridLayout {
		glm::vec2 Center = glm::vec2(0.0f);
		float Spacing = 1.0f;
		float HalfExtent = 128.0f;
		int HalfLineCount = 128;
	};

	[[nodiscard]] EditorGridLayout CalculateEditorGridLayout(const glm::vec3& cameraPosition);
}
