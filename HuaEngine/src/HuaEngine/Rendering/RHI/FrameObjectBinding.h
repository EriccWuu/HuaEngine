#pragma once

#include "glm/glm.hpp"

namespace HE::Rendering {
	struct FrameBinding {
		glm::mat4 ViewProjection = glm::mat4(1.0f);
	};

	struct ObjectBinding {
		glm::mat4 Transform = glm::mat4(1.0f);
	};
}
