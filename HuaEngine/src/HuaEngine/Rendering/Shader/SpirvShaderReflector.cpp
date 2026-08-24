#include "enginepch.h"
#include "SpirvShaderReflector.h"

#include <algorithm>
#include <charconv>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <unordered_map>

namespace {
	using namespace HE::Rendering;

	enum class TypeKind { Unknown, Int, Float, Vector, Matrix, Image, Sampler, Array, Struct, Pointer };
	struct TypeInfo {
		TypeKind Kind = TypeKind::Unknown;
		std::string Element;
		std::vector<std::string> Members;
		uint32_t Count = 0;
		uint32_t Width = 0;
		bool Signed = false;
		std::string Storage;
	};
	struct Decorations {
		std::optional<uint32_t> Set;
		std::optional<uint32_t> Binding;
		std::optional<uint32_t> Location;
		bool Builtin = false;
	};
	struct MemberDecorations {
		std::optional<uint32_t> Offset;
		uint32_t MatrixStride = 0;
		uint32_t ArrayStride = 0;
		bool RowMajor = false;
		bool ColumnMajor = false;
	};

	std::vector<std::string> Tokens(std::string_view line) {
		std::istringstream stream{ std::string(line) };
		return { std::istream_iterator<std::string>(stream), std::istream_iterator<std::string>() };
	}

	uint32_t Number(std::string_view value) {
		uint32_t result = 0;
		std::from_chars(value.data(), value.data() + value.size(), result);
		return result;
	}

	std::string Unquote(std::string_view line) {
		const auto begin = line.find('"');
		const auto end = line.rfind('"');
		return begin != std::string_view::npos && end > begin ? std::string(line.substr(begin + 1, end - begin - 1)) : std::string();
	}

	HE::ResultEnvelope Failure(std::string message);

	HE::ResultEnvelope ResolveValueType(
		const std::string& id,
		const std::unordered_map<std::string, TypeInfo>& types,
		ShaderValueType& output) {
		const auto it = types.find(id);
		if (it == types.end()) return Failure("SPIR-V value references an unknown type");
		const auto& type = it->second;
		if (type.Kind == TypeKind::Int) {
			if (type.Width != 32 || !type.Signed) return Failure("Only signed 32-bit scalar integers are supported");
			output = ShaderValueType::Int;
			return HE::ResultEnvelope::Success("shader.spirv.type", id, "SPIR-V scalar type resolved");
		}
		if (type.Kind == TypeKind::Float) {
			if (type.Width != 32) return Failure("Only 32-bit floating point values are supported");
			output = ShaderValueType::Float;
			return HE::ResultEnvelope::Success("shader.spirv.type", id, "SPIR-V scalar type resolved");
		}
		if (type.Kind == TypeKind::Vector) {
			const auto element = types.find(type.Element);
			if (element == types.end() || element->second.Kind != TypeKind::Float || element->second.Width != 32) {
				return Failure("Only floating point vectors are supported");
			}
			if (type.Count == 2) output = ShaderValueType::Float2;
			else if (type.Count == 3) output = ShaderValueType::Float3;
			else if (type.Count == 4) output = ShaderValueType::Float4;
			else return Failure("Only two, three, or four component vectors are supported");
			return HE::ResultEnvelope::Success("shader.spirv.type", id, "SPIR-V vector type resolved");
		}
		if (type.Kind == TypeKind::Matrix) {
			const auto column = types.find(type.Element);
			if (type.Count != 4 || column == types.end() || column->second.Kind != TypeKind::Vector || column->second.Count != 4) {
				return Failure("Only float4x4 matrices are supported");
			}
			const auto scalar = types.find(column->second.Element);
			if (scalar == types.end() || scalar->second.Kind != TypeKind::Float || scalar->second.Width != 32) {
				return Failure("Only float4x4 matrices are supported");
			}
			output = ShaderValueType::Float4x4;
			return HE::ResultEnvelope::Success("shader.spirv.type", id, "SPIR-V matrix type resolved");
		}
		if (type.Kind == TypeKind::Array) return Failure("Shader value arrays are not supported in the initial interface contract");
		return Failure("SPIR-V value type is not supported by the shader interface contract");
	}

	uint32_t LogicalSize(ShaderValueType type) {
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

	HE::ResultEnvelope Failure(std::string message) {
		return HE::ResultEnvelope::Failure("shader.spirv.reflect", "spirv", std::move(message));
	}
}

namespace HE::Rendering {
	ResultEnvelope ReflectSpirvAssembly(std::string_view assembly, ShaderStage stage, std::string_view entryPoint, ShaderGpuInterface& output) {
		output = {};
		output.Stages.push_back({ stage, std::string(entryPoint) });
		std::unordered_map<std::string, std::string> names;
		std::map<std::pair<std::string, uint32_t>, std::string> memberNames;
		std::unordered_map<std::string, Decorations> decorations;
		std::map<std::pair<std::string, uint32_t>, MemberDecorations> memberDecorations;
		std::unordered_map<std::string, TypeInfo> types;
		std::unordered_map<std::string, uint32_t> constants;
		std::unordered_map<std::string, std::pair<std::string, std::string>> variables;

		std::istringstream lines{ std::string(assembly) };
		std::string line;
		while (std::getline(lines, line)) {
			const auto tokens = Tokens(line);
			if (tokens.empty()) continue;
			if (tokens[0] == "OpName" && tokens.size() >= 3) names[tokens[1]] = Unquote(line);
			else if (tokens[0] == "OpMemberName" && tokens.size() >= 4) memberNames[{ tokens[1], Number(tokens[2]) }] = Unquote(line);
			else if (tokens[0] == "OpDecorate" && tokens.size() >= 3) {
				auto& value = decorations[tokens[1]];
				if (tokens[2] == "DescriptorSet" && tokens.size() >= 4) value.Set = Number(tokens[3]);
				else if (tokens[2] == "Binding" && tokens.size() >= 4) value.Binding = Number(tokens[3]);
				else if (tokens[2] == "Location" && tokens.size() >= 4) value.Location = Number(tokens[3]);
				else if (tokens[2] == "BuiltIn") value.Builtin = true;
			}
			else if (tokens[0] == "OpMemberDecorate" && tokens.size() >= 4) {
				auto& value = memberDecorations[{ tokens[1], Number(tokens[2]) }];
				if (tokens[3] == "Offset" && tokens.size() >= 5) value.Offset = Number(tokens[4]);
				else if (tokens[3] == "MatrixStride" && tokens.size() >= 5) value.MatrixStride = Number(tokens[4]);
				else if (tokens[3] == "RowMajor") value.RowMajor = true;
				else if (tokens[3] == "ColMajor") value.ColumnMajor = true;
			}
			else if (tokens.size() >= 3 && tokens[1] == "=" && tokens[2].starts_with("OpType")) {
				TypeInfo type;
				if (tokens[2] == "OpTypeInt" && tokens.size() >= 5) { type.Kind = TypeKind::Int; type.Width = Number(tokens[3]); type.Signed = Number(tokens[4]) != 0; }
				else if (tokens[2] == "OpTypeFloat" && tokens.size() >= 4) { type.Kind = TypeKind::Float; type.Width = Number(tokens[3]); }
				else if (tokens[2] == "OpTypeVector" && tokens.size() >= 5) { type.Kind = TypeKind::Vector; type.Element = tokens[3]; type.Count = Number(tokens[4]); }
				else if (tokens[2] == "OpTypeMatrix" && tokens.size() >= 5) { type.Kind = TypeKind::Matrix; type.Element = tokens[3]; type.Count = Number(tokens[4]); }
				else if (tokens[2] == "OpTypeImage") type.Kind = TypeKind::Image;
				else if (tokens[2] == "OpTypeSampler") type.Kind = TypeKind::Sampler;
				else if (tokens[2] == "OpTypeArray" && tokens.size() >= 5) { type.Kind = TypeKind::Array; type.Element = tokens[3]; type.Count = constants[tokens[4]]; }
				else if (tokens[2] == "OpTypeStruct") { type.Kind = TypeKind::Struct; type.Members.assign(tokens.begin() + 3, tokens.end()); }
				else if (tokens[2] == "OpTypePointer" && tokens.size() >= 5) { type.Kind = TypeKind::Pointer; type.Storage = tokens[3]; type.Element = tokens[4]; }
				types[tokens[0]] = std::move(type);
			}
			else if (tokens.size() >= 5 && tokens[1] == "=" && tokens[2] == "OpConstant") constants[tokens[0]] = Number(tokens[4]);
			else if (tokens.size() >= 5 && tokens[1] == "=" && tokens[2] == "OpVariable") variables[tokens[0]] = { tokens[3], tokens[4] };
		}

		const uint8_t stageMask = stage == ShaderStage::Vertex ? 1 : 2;
		for (const auto& [id, variable] : variables) {
			const auto pointer = types.find(variable.first);
			if (pointer == types.end() || pointer->second.Kind != TypeKind::Pointer) continue;
			const auto& decoration = decorations[id];
			if ((variable.second == "Input" || variable.second == "Output") && decoration.Location && !decoration.Builtin) {
				ShaderValueType valueType = ShaderValueType::Float;
				auto typeResult = ResolveValueType(pointer->second.Element, types, valueType);
				if (!typeResult.Succeeded()) return typeResult;
				const ShaderStageVariable stageVariable{ *decoration.Location, valueType };
				if (variable.second == "Input") output.Stages.front().Inputs.push_back(stageVariable);
				else output.Stages.front().Outputs.push_back(stageVariable);
			}
			if (variable.second == "Input" && stage == ShaderStage::Vertex && decoration.Location && !decoration.Builtin) {
				std::string semantic = names[id];
				if (semantic.starts_with("in.var.")) semantic.erase(0, 7);
				ShaderValueType valueType = ShaderValueType::Float;
				auto typeResult = ResolveValueType(pointer->second.Element, types, valueType);
				if (!typeResult.Succeeded()) return typeResult;
				output.VertexInputs.push_back({ semantic, *decoration.Location, valueType });
				continue;
			}
			if (!decoration.Set || !decoration.Binding) continue;
			const auto pointee = types.find(pointer->second.Element);
			if (pointee == types.end()) continue;
			ShaderResourceBinding resource;
			resource.Name = names[id];
			resource.Set = *decoration.Set;
			resource.Binding = *decoration.Binding;
			resource.StageMask = stageMask;
			const TypeInfo* resourceType = &pointee->second;
			if (resourceType->Kind == TypeKind::Array) return Failure("Shader resource arrays are not supported in the initial interface contract");
			if (variable.second == "Uniform" && resourceType->Kind == TypeKind::Struct) {
				resource.Type = ShaderResourceType::ConstantBuffer;
				ShaderConstantBuffer buffer;
				buffer.Name = resource.Name;
				buffer.Set = resource.Set;
				buffer.Binding = resource.Binding;
				for (uint32_t index = 0; index < resourceType->Members.size(); ++index) {
					const auto& memberType = resourceType->Members[index];
					const auto& memberDecoration = memberDecorations[{ pointer->second.Element, index }];
					if (!memberDecoration.Offset) return Failure("SPIR-V constant member is missing an offset");
					ShaderConstantMember member;
					member.Name = memberNames[{ pointer->second.Element, index }];
					auto typeResult = ResolveValueType(memberType, types, member.Type);
					if (!typeResult.Succeeded()) return typeResult;
					member.Offset = *memberDecoration.Offset;
					member.MatrixStride = memberDecoration.MatrixStride;
					if (member.Type == ShaderValueType::Float4x4 && member.MatrixStride != 16) {
						return Failure("Only column-major float4x4 matrices with a 16-byte stride are supported");
					}
					member.ColumnMajor = member.Type == ShaderValueType::Float4x4;
					member.Size = LogicalSize(member.Type);
					buffer.Size = std::max(buffer.Size, member.Offset + member.Size);
					buffer.Members.emplace_back(std::move(member));
				}
				buffer.Size = (buffer.Size + 15u) & ~15u;
				output.ConstantBuffers.emplace_back(std::move(buffer));
			}
			else if (resourceType->Kind == TypeKind::Image) resource.Type = ShaderResourceType::Texture2D;
			else if (resourceType->Kind == TypeKind::Sampler) resource.Type = ShaderResourceType::Sampler;
			else continue;
			output.Resources.emplace_back(std::move(resource));
		}
		return ResultEnvelope::Success("shader.spirv.reflect", "spirv", "SPIR-V interface reflected");
	}

	ResultEnvelope MergeShaderStageInterfaces(const ShaderGpuInterface& vertex, const ShaderGpuInterface& fragment, ShaderGpuInterface& output) {
		if (vertex.Stages.size() != 1 || fragment.Stages.size() != 1) return Failure("Shader stage interface count is invalid");
		for (const auto& input : fragment.Stages.front().Inputs) {
			const auto matching = std::find_if(vertex.Stages.front().Outputs.begin(), vertex.Stages.front().Outputs.end(), [&](const auto& value) { return value.Location == input.Location; });
			if (matching == vertex.Stages.front().Outputs.end() || matching->Type != input.Type) return Failure("Vertex output and fragment input interfaces are incompatible");
		}
		output = vertex;
		output.Stages.insert(output.Stages.end(), fragment.Stages.begin(), fragment.Stages.end());
		for (const auto& resource : fragment.Resources) {
			auto existing = std::find_if(output.Resources.begin(), output.Resources.end(), [&](const auto& value) { return value.Set == resource.Set && value.Binding == resource.Binding; });
			if (existing == output.Resources.end()) output.Resources.push_back(resource);
			else if (existing->Type != resource.Type || existing->ArrayCount != resource.ArrayCount || existing->Name != resource.Name) return Failure("Shader stages declare incompatible resource bindings");
			else existing->StageMask |= resource.StageMask;
		}
		for (const auto& buffer : fragment.ConstantBuffers) {
			auto existing = std::find_if(output.ConstantBuffers.begin(), output.ConstantBuffers.end(), [&](const auto& value) { return value.Set == buffer.Set && value.Binding == buffer.Binding; });
			if (existing == output.ConstantBuffers.end()) output.ConstantBuffers.push_back(buffer);
			else if (existing->Size != buffer.Size || existing->Members.size() != buffer.Members.size()) return Failure("Shader stages declare incompatible constant buffers");
			else {
				for (size_t index = 0; index < buffer.Members.size(); ++index) {
					const auto& a = existing->Members[index]; const auto& b = buffer.Members[index];
					if (a.Name != b.Name || a.Type != b.Type || a.Offset != b.Offset || a.Size != b.Size || a.MatrixStride != b.MatrixStride || a.ColumnMajor != b.ColumnMajor) return Failure("Shader stages declare incompatible constant buffer layouts");
				}
			}
		}
		return ResultEnvelope::Success("shader.interface.merge", "shader-interface", "Shader stage interfaces merged");
	}
}
