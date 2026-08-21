#pragma once

#include <optional>
#include <string_view>

#include "HuaEngine/Rendering/RenderCamera.h"

namespace HE::Rendering {
	struct RenderFrameData {
		static constexpr std::string_view ResourceName = "Rendering.FrameData";

		std::optional<RenderCamera> ActiveCamera;
	};
}
