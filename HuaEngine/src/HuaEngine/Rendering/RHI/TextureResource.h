#pragma once

#include <cstdint>
#include <string>

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Rendering/RHI/RenderTargetTypes.h"

namespace HE::Rendering {
	class TextureResource;

	using TextureUsageFlags = uint32_t;

	constexpr TextureUsageFlags TextureUsageNone = 0;
	constexpr TextureUsageFlags TextureUsageSampled = 1 << 0;
	constexpr TextureUsageFlags TextureUsageColorAttachment = 1 << 1;
	constexpr TextureUsageFlags TextureUsageDepthStencilAttachment = 1 << 2;
	constexpr TextureUsageFlags TextureUsageCopySrc = 1 << 3;
	constexpr TextureUsageFlags TextureUsageCopyDst = 1 << 4;

	struct TextureDesc {
		uint32_t Width = 0;
		uint32_t Height = 0;
		RenderTargetTextureFormat Format = RenderTargetTextureFormat::None;
		TextureUsageFlags Usage = TextureUsageNone;
		uint32_t MipLevels = 1;
		uint32_t Samples = 1;
		std::string SourcePath;
	};

	struct TextureViewDesc {
		Ref<TextureResource> Texture;
		RenderTargetTextureFormat Format = RenderTargetTextureFormat::None;
		uint32_t BaseMipLevel = 0;
		uint32_t MipLevelCount = 1;
	};

	enum class SamplerFilter : uint8_t {
		Nearest = 0,
		Linear
	};

	enum class SamplerAddressMode : uint8_t {
		Repeat = 0,
		ClampToEdge
	};

	struct SamplerDesc {
		SamplerFilter MinFilter = SamplerFilter::Linear;
		SamplerFilter MagFilter = SamplerFilter::Linear;
		SamplerAddressMode AddressU = SamplerAddressMode::Repeat;
		SamplerAddressMode AddressV = SamplerAddressMode::Repeat;
		SamplerAddressMode AddressW = SamplerAddressMode::Repeat;
	};

	class TextureResource {
	public:
		virtual ~TextureResource() = default;

		virtual const TextureDesc& GetDesc() const = 0;
		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
	};

	class TextureView {
	public:
		virtual ~TextureView() = default;

		virtual const TextureViewDesc& GetDesc() const = 0;
	};

	class Sampler {
	public:
		virtual ~Sampler() = default;

		virtual const SamplerDesc& GetDesc() const = 0;
	};
}
