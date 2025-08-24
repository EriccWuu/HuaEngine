#pragma once

#include "Buffer.h"
#include <cstdint>

namespace HE::Rendering {
	class IndexBuffer : public Buffer {
	public:
		virtual ~IndexBuffer() {}
		static Ref<IndexBuffer> Create(uint32_t* indices, uint32_t count);
		virtual uint32_t GetCount() const = 0;
	};
}