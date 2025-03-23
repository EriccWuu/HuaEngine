#pragma once 

namespace HE {
	class Buffer {
	public:
		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;
	};
}