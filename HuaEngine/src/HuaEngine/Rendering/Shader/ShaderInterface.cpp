#include "enginepch.h"
#include "ShaderInterface.h"

#include <algorithm>
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
}

namespace HE::Rendering {
	ResultEnvelope FinalizeShaderInterface(ShaderInterface& shaderInterface) {
		auto& gpu = shaderInterface.Gpu;
		std::set<std::pair<uint32_t, uint32_t>> bindings;
		for (const auto& resource : gpu.Resources) {
			if (resource.Name.empty() || resource.ArrayCount == 0 || resource.StageMask == 0) return Failure("Shader resource binding is invalid");
			if (!bindings.emplace(resource.Set, resource.Binding).second) return Failure("Shader resource binding is duplicated");
		}
		for (const auto& buffer : gpu.ConstantBuffers) {
			if (buffer.Name.empty() || !bindings.contains({ buffer.Set, buffer.Binding })) return Failure("Constant buffer binding is missing from resources");
			for (const auto& member : buffer.Members) {
				if (member.Name.empty() || member.Size == 0 || member.Offset + member.Size > buffer.Size) return Failure("Constant buffer member is out of range");
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
