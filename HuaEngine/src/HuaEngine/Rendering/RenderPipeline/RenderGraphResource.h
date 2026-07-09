#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#include "HuaEngine/Rendering/RHI/RenderTargetTypes.h"

namespace HE::Rendering {
	enum class RenderGraphResourceKind : uint8_t {
		Texture = 0,
		Buffer
	};

	enum class RenderGraphResourceStorage : uint8_t {
		Imported = 0,
		Transient
	};

	struct RenderGraphResourceHandle {
		uint32_t Index = std::numeric_limits<uint32_t>::max();

		[[nodiscard]] bool IsValid() const {
			return Index != std::numeric_limits<uint32_t>::max();
		}
	};

	struct RenderGraphTextureDesc {
		uint32_t Width = 0;
		uint32_t Height = 0;
		RenderTargetTextureFormat Format = RenderTargetTextureFormat::RGBA8;
	};

	struct RenderGraphBufferDesc {
		uint32_t Size = 0;
		uint32_t Stride = 0;
	};

	struct RenderGraphResourceDesc {
		std::string Name;
		RenderGraphResourceKind Kind = RenderGraphResourceKind::Texture;
		RenderGraphResourceStorage Storage = RenderGraphResourceStorage::Transient;
		RenderGraphTextureDesc Texture;
		RenderGraphBufferDesc Buffer;
	};

	struct RenderGraphResourceLifetime {
		RenderGraphResourceHandle Handle;
		std::string Name;
		uint32_t FirstPassIndex = 0;
		uint32_t LastPassIndex = 0;
		RenderGraphResourceStorage Storage = RenderGraphResourceStorage::Transient;
	};

	class RenderGraphResourceAllocator {
	public:
		RenderGraphResourceHandle AddImportedResource(RenderGraphResourceDesc desc);
		RenderGraphResourceHandle AddTransientResource(RenderGraphResourceDesc desc);
		void Reset();
		void ClearLifetimes();
		void SetLifetime(RenderGraphResourceHandle handle, uint32_t firstPassIndex, uint32_t lastPassIndex);

		[[nodiscard]] const RenderGraphResourceDesc* GetDesc(RenderGraphResourceHandle handle) const;
		[[nodiscard]] RenderGraphResourceHandle FindByName(const std::string& name) const;
		[[nodiscard]] const std::vector<RenderGraphResourceDesc>& GetResources() const { return m_Resources; }
		[[nodiscard]] const std::vector<RenderGraphResourceLifetime>& GetLifetimes() const { return m_Lifetimes; }

	private:
		RenderGraphResourceHandle AddResource(RenderGraphResourceDesc desc, RenderGraphResourceStorage storage);

		std::vector<RenderGraphResourceDesc> m_Resources;
		std::vector<RenderGraphResourceLifetime> m_Lifetimes;
		std::unordered_map<std::string, uint32_t> m_NameToIndex;
	};
}
