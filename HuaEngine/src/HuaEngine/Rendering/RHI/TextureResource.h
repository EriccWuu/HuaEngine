#pragma once

#include <cstdint>
#include <string>

namespace HE::Rendering {
	struct TextureDesc {
		std::string SourcePath;
	};

	class TextureResource {
	public:
		virtual ~TextureResource() = default;

		virtual const TextureDesc& GetDesc() const = 0;
		virtual uint32_t GetRenderID() const = 0;
		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
	};
}
