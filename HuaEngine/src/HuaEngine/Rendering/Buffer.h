#pragma once 

namespace HE::Rendering {
	class Buffer {
	public:
		// Legacy shell compatibility helper.
		// New render passes should submit state through RHI CommandList.
		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;
	};
}
