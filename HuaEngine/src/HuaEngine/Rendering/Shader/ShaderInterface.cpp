#include "enginepch.h"
#include "ShaderInterface.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>

namespace {
	class CanonicalWriter final {
	public:
		void U8(uint8_t value) { m_Bytes.push_back(value); }
		void U32(uint32_t value) { for (uint32_t shift = 0; shift < 32; shift += 8) U8(static_cast<uint8_t>(value >> shift)); }
		void Float(float value) { uint32_t bits = 0; std::memcpy(&bits, &value, sizeof(bits)); U32(bits); }
		void String(std::string_view value) { U32(static_cast<uint32_t>(value.size())); m_Bytes.insert(m_Bytes.end(), value.begin(), value.end()); }
		[[nodiscard]] const std::vector<uint8_t>& Bytes() const { return m_Bytes; }
	private:
		std::vector<uint8_t> m_Bytes;
	};

	void WriteValue(CanonicalWriter& writer, const HE::Rendering::ShaderParameterValue& value) {
		writer.U8(static_cast<uint8_t>(value.index()));
		std::visit([&](const auto& item) {
			using T = std::decay_t<decltype(item)>;
			if constexpr (std::is_same_v<T, int32_t>) writer.U32(static_cast<uint32_t>(item));
			else if constexpr (std::is_same_v<T, float>) writer.Float(item);
			else if constexpr (std::is_same_v<T, std::string>) writer.String(item);
			else {
				const auto* values = reinterpret_cast<const float*>(&item);
				for (size_t index = 0; index < sizeof(T) / sizeof(float); ++index) writer.Float(values[index]);
			}
		}, value);
	}

	HE::ResultEnvelope Failure(std::string message) {
		return HE::ResultEnvelope::Failure("shader.interface.finalize", "shader-interface", std::move(message));
	}

	bool IsValidValueType(HE::Rendering::ShaderValueType type) {
		using HE::Rendering::ShaderValueType;
		switch (type) {
		case ShaderValueType::Int:
		case ShaderValueType::Float:
		case ShaderValueType::Float2:
		case ShaderValueType::Float3:
		case ShaderValueType::Float4:
		case ShaderValueType::Float4x4:
		case ShaderValueType::Texture2D:
		case ShaderValueType::SamplerState:
			return true;
		}
		return false;
	}

	bool IsStageValueType(HE::Rendering::ShaderValueType type) {
		using HE::Rendering::ShaderValueType;
		return type == ShaderValueType::Int || type == ShaderValueType::Float ||
			type == ShaderValueType::Float2 || type == ShaderValueType::Float3 || type == ShaderValueType::Float4;
	}

	uint32_t ConstantValueSize(HE::Rendering::ShaderValueType type) {
		using HE::Rendering::ShaderValueType;
		switch (type) {
		case ShaderValueType::Int:
		case ShaderValueType::Float: return 4;
		case ShaderValueType::Float2: return 8;
		case ShaderValueType::Float3: return 12;
		case ShaderValueType::Float4: return 16;
		case ShaderValueType::Float4x4: return 64;
		default: return 0;
		}
	}

	bool DefaultValueMatches(
		HE::Rendering::ShaderValueType type,
		const HE::Rendering::ShaderParameterValue& value) {
		using namespace HE::Rendering;
		switch (type) {
		case ShaderValueType::Int: return std::holds_alternative<int32_t>(value);
		case ShaderValueType::Float: return std::holds_alternative<float>(value);
		case ShaderValueType::Float2: return std::holds_alternative<glm::vec2>(value);
		case ShaderValueType::Float3: return std::holds_alternative<glm::vec3>(value);
		case ShaderValueType::Float4: return std::holds_alternative<glm::vec4>(value);
		case ShaderValueType::Float4x4: return std::holds_alternative<glm::mat4>(value);
		case ShaderValueType::Texture2D:
		case ShaderValueType::SamplerState: return std::holds_alternative<std::string>(value);
		}
		return false;
	}
}

namespace HE::Rendering {
	ResultEnvelope ValidateShaderGpuInterface(const ShaderGpuInterface& gpu) {
		if (gpu.Stages.size() != 2) return Failure("Shader interface must contain one vertex and one fragment stage");
		std::set<ShaderStage> stageKinds;
		for (const auto& stage : gpu.Stages) {
			if ((stage.Stage != ShaderStage::Vertex && stage.Stage != ShaderStage::Fragment) ||
				stage.EntryPoint.empty() || !stageKinds.emplace(stage.Stage).second) {
				return Failure("Shader stage contract is invalid or duplicated");
			}
			for (const auto* variables : { &stage.Inputs, &stage.Outputs }) {
				std::set<uint32_t> locations;
				for (const auto& variable : *variables) {
					if (!IsStageValueType(variable.Type) || !locations.emplace(variable.Location).second) {
						return Failure("Shader stage variable contract is invalid");
					}
				}
			}
		}

		std::set<uint32_t> vertexLocations;
		for (const auto& input : gpu.VertexInputs) {
			if (input.Semantic.empty() || !IsStageValueType(input.Type) || !vertexLocations.emplace(input.Location).second) {
				return Failure("Shader vertex input contract is invalid");
			}
		}

		std::set<std::pair<uint32_t, uint32_t>> bindings;
		for (const auto& resource : gpu.Resources) {
			const bool validType = resource.Type == ShaderResourceType::ConstantBuffer ||
				resource.Type == ShaderResourceType::Texture2D || resource.Type == ShaderResourceType::Sampler;
			if (resource.Name.empty() || !validType || resource.Set > 2 || resource.ArrayCount != 1 ||
				resource.StageMask == 0 || (resource.StageMask & ~uint8_t{ 3 }) != 0 ||
				!bindings.emplace(resource.Set, resource.Binding).second) {
				return Failure("Shader resource binding contract is invalid or duplicated");
			}
		}

		std::set<std::pair<uint32_t, uint32_t>> constantBufferBindings;
		for (const auto& buffer : gpu.ConstantBuffers) {
			const auto resource = std::find_if(gpu.Resources.begin(), gpu.Resources.end(), [&](const auto& value) {
				return value.Set == buffer.Set && value.Binding == buffer.Binding;
			});
			if (buffer.Name.empty() || buffer.Set > 2 || buffer.Size == 0 ||
				resource == gpu.Resources.end() || resource->Type != ShaderResourceType::ConstantBuffer ||
				resource->Name != buffer.Name || !constantBufferBindings.emplace(buffer.Set, buffer.Binding).second) {
				return Failure("Shader constant buffer binding contract is invalid");
			}
			std::set<std::string> memberNames;
			std::vector<std::pair<uint32_t, uint32_t>> ranges;
			for (const auto& member : buffer.Members) {
				const auto expectedSize = ConstantValueSize(member.Type);
				const bool matrix = member.Type == ShaderValueType::Float4x4;
				if (member.Name.empty() || expectedSize == 0 || member.Size != expectedSize ||
					member.Offset > buffer.Size || member.Size > buffer.Size - member.Offset ||
					member.ArrayStride != 0 || (matrix ? member.MatrixStride != 16 || !member.ColumnMajor : member.MatrixStride != 0 || member.ColumnMajor) ||
					!memberNames.emplace(member.Name).second) {
					return Failure("Shader constant member contract is invalid");
				}
				const auto range = std::pair{ member.Offset, member.Offset + member.Size };
				if (std::any_of(ranges.begin(), ranges.end(), [&](const auto& value) { return range.first < value.second && value.first < range.second; })) {
					return Failure("Shader constant members overlap");
				}
				ranges.push_back(range);
			}
		}
		return ResultEnvelope::Success("shader.interface.validate", "shader-interface", "Shader GPU interface validated");
	}

	ResultEnvelope FinalizeShaderInterface(ShaderInterface& shaderInterface) {
		auto& gpu = shaderInterface.Gpu;
		auto validation = ValidateShaderGpuInterface(gpu);
		if (!validation.Succeeded()) return validation;
		std::set<std::string> parameterNames;
		for (const auto& parameter : shaderInterface.Authoring.Parameters) {
			const bool validScope = parameter.Scope == ShaderParameterScope::Frame ||
				parameter.Scope == ShaderParameterScope::Material || parameter.Scope == ShaderParameterScope::Object;
			const bool validEditor = parameter.Editor == ShaderEditorKind::Default ||
				parameter.Editor == ShaderEditorKind::Color || parameter.Editor == ShaderEditorKind::Texture2D;
			const bool validRange = parameter.Range.empty() ||
				(parameter.Range.size() == 2 && std::isfinite(parameter.Range[0]) && std::isfinite(parameter.Range[1]) && parameter.Range[0] <= parameter.Range[1]);
			if (parameter.Name.empty() || !IsValidValueType(parameter.Type) || !validScope || !validEditor ||
				!DefaultValueMatches(parameter.Type, parameter.DefaultValue) || !validRange ||
				!std::isfinite(parameter.Step) || parameter.Step < 0.0f || !parameterNames.emplace(parameter.Name).second) {
				return Failure("Shader authoring parameter contract is invalid");
			}
		}

		auto stages = gpu.Stages;
		auto inputs = gpu.VertexInputs;
		auto resources = gpu.Resources;
		auto buffers = gpu.ConstantBuffers;
		std::sort(stages.begin(), stages.end(), [](const auto& a, const auto& b) { return a.Stage < b.Stage; });
		std::sort(inputs.begin(), inputs.end(), [](const auto& a, const auto& b) { return a.Location < b.Location; });
		std::sort(resources.begin(), resources.end(), [](const auto& a, const auto& b) { return std::tie(a.Set, a.Binding, a.Name) < std::tie(b.Set, b.Binding, b.Name); });
		std::sort(buffers.begin(), buffers.end(), [](const auto& a, const auto& b) { return std::tie(a.Set, a.Binding, a.Name) < std::tie(b.Set, b.Binding, b.Name); });

		CanonicalWriter gpuWriter;
		gpuWriter.U32(1);
		gpuWriter.U32(static_cast<uint32_t>(stages.size()));
		for (auto& stage : stages) {
			std::sort(stage.Inputs.begin(), stage.Inputs.end(), [](const auto& a, const auto& b) { return a.Location < b.Location; });
			std::sort(stage.Outputs.begin(), stage.Outputs.end(), [](const auto& a, const auto& b) { return a.Location < b.Location; });
			gpuWriter.U8(static_cast<uint8_t>(stage.Stage)); gpuWriter.String(stage.EntryPoint);
			gpuWriter.U32(static_cast<uint32_t>(stage.Inputs.size()));
			for (const auto& input : stage.Inputs) { gpuWriter.U32(input.Location); gpuWriter.U8(static_cast<uint8_t>(input.Type)); }
			gpuWriter.U32(static_cast<uint32_t>(stage.Outputs.size()));
			for (const auto& output : stage.Outputs) { gpuWriter.U32(output.Location); gpuWriter.U8(static_cast<uint8_t>(output.Type)); }
		}
		gpuWriter.U32(static_cast<uint32_t>(inputs.size()));
		for (const auto& input : inputs) { gpuWriter.String(input.Semantic); gpuWriter.U32(input.Location); gpuWriter.U8(static_cast<uint8_t>(input.Type)); }
		gpuWriter.U32(static_cast<uint32_t>(resources.size()));
		for (const auto& resource : resources) {
			gpuWriter.String(resource.Name); gpuWriter.U8(static_cast<uint8_t>(resource.Type)); gpuWriter.U32(resource.Set);
			gpuWriter.U32(resource.Binding); gpuWriter.U32(resource.ArrayCount); gpuWriter.U8(resource.StageMask);
		}
		gpuWriter.U32(static_cast<uint32_t>(buffers.size()));
		for (auto& buffer : buffers) {
			std::sort(buffer.Members.begin(), buffer.Members.end(), [](const auto& a, const auto& b) { return std::tie(a.Offset, a.Name) < std::tie(b.Offset, b.Name); });
			gpuWriter.String(buffer.Name); gpuWriter.U32(buffer.Set); gpuWriter.U32(buffer.Binding); gpuWriter.U32(buffer.Size);
			gpuWriter.U32(static_cast<uint32_t>(buffer.Members.size()));
			for (const auto& member : buffer.Members) {
				gpuWriter.String(member.Name); gpuWriter.U8(static_cast<uint8_t>(member.Type)); gpuWriter.U32(member.Offset); gpuWriter.U32(member.Size);
				gpuWriter.U32(member.MatrixStride); gpuWriter.U32(member.ArrayStride); gpuWriter.U8(member.ColumnMajor ? 1 : 0);
			}
		}
		gpu.Digest = ComputeSha256(gpuWriter.Bytes());
		gpu.Signature = Sha256Prefix64(gpu.Digest);

		auto parameters = shaderInterface.Authoring.Parameters;
		std::sort(parameters.begin(), parameters.end(), [](const auto& a, const auto& b) { return a.Name < b.Name; });
		CanonicalWriter authoringWriter;
		authoringWriter.U32(1);
		for (const auto byte : gpu.Digest) authoringWriter.U8(byte);
		authoringWriter.U32(static_cast<uint32_t>(parameters.size()));
		for (const auto& parameter : parameters) {
			authoringWriter.String(parameter.Name); authoringWriter.U8(static_cast<uint8_t>(parameter.Type));
			authoringWriter.U8(static_cast<uint8_t>(parameter.Editor)); WriteValue(authoringWriter, parameter.DefaultValue);
			authoringWriter.U32(static_cast<uint32_t>(parameter.Range.size()));
			for (const auto value : parameter.Range) authoringWriter.Float(value);
			authoringWriter.Float(parameter.Step);
		}
		shaderInterface.Authoring.Digest = ComputeSha256(authoringWriter.Bytes());
		shaderInterface.Authoring.Signature = Sha256Prefix64(shaderInterface.Authoring.Digest);
		return ResultEnvelope::Success("shader.interface.finalize", "shader-interface", "Shader interface finalized");
	}
}
