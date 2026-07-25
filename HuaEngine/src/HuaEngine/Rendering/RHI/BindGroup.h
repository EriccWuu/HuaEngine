#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "glm/glm.hpp"

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Rendering/RHI/GpuBuffer.h"
#include "HuaEngine/Rendering/RHI/TextureResource.h"

namespace HE::Rendering {
	enum class BindGroupScope : uint8_t {
		Frame = 0,
		Material,
		Object
	};

	enum class BindingValueType : uint8_t {
		Int = 0,
		Float,
		Float2,
		Float3,
		Float4,
		Mat3,
		Mat4,
		IntArray,
		Texture,
		TextureView,
		Sampler,
		UniformBuffer,
		StorageBuffer
	};

	using ShaderStageFlags = uint8_t;
	constexpr ShaderStageFlags ShaderStageNone = 0;
	constexpr ShaderStageFlags ShaderStageVertex = 1 << 0;
	constexpr ShaderStageFlags ShaderStageFragment = 1 << 1;
	constexpr ShaderStageFlags ShaderStageCompute = 1 << 2;
	constexpr ShaderStageFlags ShaderStageAll = ShaderStageVertex | ShaderStageFragment | ShaderStageCompute;

	using BindingValue = std::variant<
		int,
		float,
		glm::vec2,
		glm::vec3,
		glm::vec4,
		glm::mat3,
		glm::mat4,
		std::vector<int>,
		Ref<TextureResource>,
		Ref<TextureView>,
		Ref<Sampler>
		, Ref<GpuBuffer>
	>;

	struct BindGroupLayoutEntry {
		std::string Name;
		BindingValueType Type = BindingValueType::Float;
		uint32_t Binding = 0;
		ShaderStageFlags Visibility = ShaderStageAll;
		uint32_t MinBindingSize = 0;
	};

	struct BindGroupEntry {
		std::string Name;
		BindingValueType Type = BindingValueType::Float;
		BindingValue Value = 0.0f;
		uint32_t Binding = 0;
		uint32_t TextureSlot = 0;
		uint32_t Offset = 0;
		uint32_t Size = 0;
	};

	struct BindGroupLayoutDesc {
		BindGroupScope Scope = BindGroupScope::Material;
		std::vector<BindGroupLayoutEntry> Entries;
	};

	inline uint64_t CalculateBindGroupLayoutSignature(const BindGroupLayoutDesc& desc) {
		uint64_t signature = 1469598103934665603ull;
		const auto append = [&signature](uint64_t value) {
			signature ^= value;
			signature *= 1099511628211ull;
		};
		append(static_cast<uint64_t>(desc.Scope));
		for (const auto& entry : desc.Entries) {
			for (const auto character : entry.Name) append(static_cast<uint8_t>(character));
			append(static_cast<uint64_t>(entry.Type));
			append(entry.Binding);
			append(entry.Visibility);
			append(entry.MinBindingSize);
		}
		return signature;
	}

	class BindGroupLayout {
	public:
		virtual ~BindGroupLayout() = default;

		virtual const BindGroupLayoutDesc& GetDesc() const = 0;
	};

	struct BindGroupDesc {
		Ref<BindGroupLayout> Layout;
		std::vector<BindGroupEntry> Entries;
	};

	class BindGroup {
	public:
		virtual ~BindGroup() = default;

		virtual const BindGroupDesc& GetDesc() const = 0;
	};
}
