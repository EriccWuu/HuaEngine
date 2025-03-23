#pragma once

#include "Buffer.h"
#include <cstdint>

namespace HE {
	class IndexBuffer : public Buffer {
	public:
		virtual ~IndexBuffer() {}
		static IndexBuffer* Create(uint32_t* indices, uint32_t count);
		virtual uint32_t GetCount() const = 0;
	};
}