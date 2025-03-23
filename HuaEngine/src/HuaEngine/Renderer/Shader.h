#pragma once

namespace HE {
	class Shader {
	public:
		virtual void Bind() = 0;
		virtual void Unbind() = 0;
	};
}