#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "glm/glm.hpp"

#include "HuaEngine/Core/Core.h"
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
		Sampler
	};

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
	>;

	struct BindGroupLayoutEntry {
		std::string Name;
		BindingValueType Type = BindingValueType::Float;
		uint32_t Binding = 0;
	};

	struct BindGroupEntry {
		std::string Name;
		BindingValueType Type = BindingValueType::Float;
		BindingValue Value = 0.0f;
		uint32_t Binding = 0;
		uint32_t TextureSlot = 0;
	};

	struct BindGroupLayoutDesc {
		BindGroupScope Scope = BindGroupScope::Material;
		std::vector<BindGroupLayoutEntry> Entries;
	};

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
