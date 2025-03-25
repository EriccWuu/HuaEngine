#pragma once

#include <cstdint>
#include "HuaEngine/Core/Core.h"
#include "stb_image.h"

namespace HE{
	class Texture {
	public:
		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		virtual void Bind(uint32_t slot = 0) = 0;
	};

	class Texture2D : public Texture {
	public:
		static Ref<Texture2D> Create(const std::string& path);
	};
}
