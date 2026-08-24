#include "enginepch.h"
#include "EditorGrid.h"

#include <cmath>

namespace HE::Editor {
	EditorGridLayout CalculateEditorGridLayout(const glm::vec3& cameraPosition) {
		constexpr int halfLineCount = 128;
		constexpr float linesPerCameraHeight = 16.0f;

		const float cameraHeight = std::max(std::abs(cameraPosition.y), 1.0f);
		const float spacingExponent = std::max(0.0f, std::floor(std::log10(cameraHeight / linesPerCameraHeight)));
		const float spacing = std::pow(10.0f, spacingExponent);
		const glm::vec2 center(
			std::round(cameraPosition.x / spacing) * spacing,
			std::round(cameraPosition.z / spacing) * spacing);
		return {
			.Center = center,
			.Spacing = spacing,
			.HalfExtent = spacing * static_cast<float>(halfLineCount),
			.HalfLineCount = halfLineCount
		};
	}
}
