#pragma once

#include <iostream>
#include <glm/glm.hpp>

namespace HE {
	inline std::ostream& operator<<(std::ostream& os, glm::vec3 vec3) {
		return os << "(" << vec3.x << ", " << vec3.y << ", " << vec3.z << ")\n";
	}
}