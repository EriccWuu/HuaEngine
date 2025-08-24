#pragma once 

namespace HE::Rendering {
	class Buffer {
	public:
		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;
	};
}