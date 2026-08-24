#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include <glm/glm.hpp>

#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Core/Sha256.h"

namespace HE::Rendering {
	enum class ShaderStage : uint8_t { Vertex, Fragment };
	enum class ShaderValueType : uint8_t { Int, Float, Float2, Float3, Float4, Float4x4, Texture2D, SamplerState };
	enum class ShaderResourceType : uint8_t { ConstantBuffer, Texture2D, Sampler };
	enum class ShaderParameterScope : uint8_t { Frame, Material, Object };
	enum class ShaderEditorKind : uint8_t { Default, Color, Texture2D };

	using ShaderParameterValue = std::variant<int32_t, float, glm::vec2, glm::vec3, glm::vec4, glm::mat4, std::string>;

	struct ShaderStageVariable {
		uint32_t Location = 0;
		ShaderValueType Type = ShaderValueType::Float;
	};

	struct ShaderStageEntry {
		ShaderStage Stage = ShaderStage::Vertex;
		std::string EntryPoint;
		std::vector<ShaderStageVariable> Inputs;
		std::vector<ShaderStageVariable> Outputs;
	};

	struct ShaderVertexInput {
		std::string Semantic;
		uint32_t Location = 0;
		ShaderValueType Type = ShaderValueType::Float;
	};

	struct ShaderResourceBinding {
		std::string Name;
		ShaderResourceType Type = ShaderResourceType::ConstantBuffer;
		uint32_t Set = 0;
		uint32_t Binding = 0;
		uint32_t ArrayCount = 1;
		uint8_t StageMask = 0;
	};

	struct ShaderConstantMember {
		std::string Name;
		ShaderValueType Type = ShaderValueType::Float;
		uint32_t Offset = 0;
		uint32_t Size = 0;
		uint32_t MatrixStride = 0;
		uint32_t ArrayStride = 0;
		bool ColumnMajor = false;
	};

	struct ShaderConstantBuffer {
		std::string Name;
		uint32_t Set = 0;
		uint32_t Binding = 0;
		uint32_t Size = 0;
		std::vector<ShaderConstantMember> Members;
	};

	struct ShaderParameterMetadata {
		std::string Name;
		std::string DisplayName;
		ShaderParameterScope Scope = ShaderParameterScope::Material;
		ShaderValueType Type = ShaderValueType::Float;
		ShaderEditorKind Editor = ShaderEditorKind::Default;
		ShaderParameterValue DefaultValue = 0.0f;
		std::vector<float> Range;
		float Step = 0.0f;
		std::string Tooltip;
	};

	struct ShaderGpuInterface {
		std::vector<ShaderStageEntry> Stages;
		std::vector<ShaderVertexInput> VertexInputs;
		std::vector<ShaderResourceBinding> Resources;
		std::vector<ShaderConstantBuffer> ConstantBuffers;
		Sha256Digest Digest{};
		uint64_t Signature = 0;
	};

	struct ShaderAuthoringMetadata {
		std::vector<ShaderParameterMetadata> Parameters;
		Sha256Digest Digest{};
		uint64_t Signature = 0;
	};

	struct ShaderInterface {
		ShaderGpuInterface Gpu;
		ShaderAuthoringMetadata Authoring;
	};

	ResultEnvelope FinalizeShaderInterface(ShaderInterface& shaderInterface);
}
