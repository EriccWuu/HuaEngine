#pragma once

#include <string>

namespace HE::Rendering {
	struct ShaderProgramDesc {
		std::string VertexSource;
		std::string FragmentSource;
	};

	class ShaderProgram {
	public:
		virtual ~ShaderProgram() = default;

		virtual const ShaderProgramDesc& GetDesc() const = 0;
	};
}
