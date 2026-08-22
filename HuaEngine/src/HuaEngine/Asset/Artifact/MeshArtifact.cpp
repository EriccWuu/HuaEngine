#include "enginepch.h"
#include "MeshArtifact.h"

#include <algorithm>
#include <limits>

#include "HuaEngine/Asset/Library/AssetBinaryIO.h"

namespace {
	constexpr uint32_t MaxLayoutElementCount = 64;
	constexpr uint32_t MaxVertexFloatCount = 256 * 1024 * 1024;
	constexpr uint32_t MaxIndexCount = 256 * 1024 * 1024;

	bool IsValidShaderDataType(uint8_t value) {
		return value >= static_cast<uint8_t>(HE::Rendering::ShaderDataType::Float) &&
			value <= static_cast<uint8_t>(HE::Rendering::ShaderDataType::Bool);
	}

	bool IsMeshDataConsistent(const HE::Rendering::MeshData& data) {
		if (!data.IsValid() || data.Layout.Stride == 0 || data.Layout.Elements.size() > MaxLayoutElementCount) {
			return false;
		}
		const auto vertexBytes = data.VertexData.size() * sizeof(float);
		if (vertexBytes % data.Layout.Stride != 0) {
			return false;
		}
		const auto vertexCount = vertexBytes / data.Layout.Stride;
		return vertexCount > 0 && std::all_of(data.IndexData.begin(), data.IndexData.end(), [&](uint32_t index) {
			return index < vertexCount;
		});
	}

	HE::ResultEnvelope MakeMeshArtifactFailure(std::string operation, std::string code, std::string message) {
		auto result = HE::ResultEnvelope::Failure(std::move(operation), "asset:mesh", message);
		result.AddDetail({ HE::DiagnosticSeverity::Error, std::move(code), std::move(message), {} });
		return result;
	}
}

namespace HE {
	ResultEnvelope EncodeMeshArtifact(
		const Rendering::Mesh& mesh,
		AssetArtifact& outArtifact) {
		outArtifact = {};
		const auto& data = mesh.GetMeshData();
		if (!IsMeshDataConsistent(data) ||
			data.VertexData.size() > std::numeric_limits<uint32_t>::max() ||
			data.IndexData.size() > std::numeric_limits<uint32_t>::max()) {
			return MakeMeshArtifactFailure("asset.mesh_artifact.encode", "asset.mesh_artifact.invalid_data", "Mesh data is invalid or exceeds artifact limits");
		}

		AssetBinaryWriter writer;
		writer.WriteString(mesh.GetName());
		writer.WriteU32(data.Layout.Stride);
		writer.WriteU32(static_cast<uint32_t>(data.Layout.Elements.size()));
		for (const auto& element : data.Layout.Elements) {
			if (!IsValidShaderDataType(element.Type)) {
				return MakeMeshArtifactFailure("asset.mesh_artifact.encode", "asset.mesh_artifact.layout_invalid", "Mesh layout contains an invalid shader data type");
			}
			writer.WriteU8(element.Type);
			writer.WriteString(element.Name);
			writer.WriteU32(element.Size);
			writer.WriteU32(element.Offset);
			writer.WriteU8(element.Normalized ? 1 : 0);
		}

		writer.WriteU32(static_cast<uint32_t>(data.VertexData.size()));
		for (float value : data.VertexData) {
			writer.WriteFloat(value);
		}
		writer.WriteU32(static_cast<uint32_t>(data.IndexData.size()));
		for (uint32_t value : data.IndexData) {
			writer.WriteU32(value);
		}

		outArtifact.Kind = AssetKind::Mesh;
		outArtifact.ArtifactVersion = MeshArtifactVersion;
		outArtifact.Payload = writer.TakeData();
		return ResultEnvelope::Success("asset.mesh_artifact.encode", mesh.GetName(), "Mesh artifact encoded");
	}

	ResultEnvelope DecodeMeshArtifact(
		const AssetArtifact& artifact,
		Ref<Rendering::Mesh>& outMesh) {
		outMesh = nullptr;
		if (artifact.Kind != AssetKind::Mesh || artifact.ArtifactVersion != MeshArtifactVersion) {
			return MakeMeshArtifactFailure("asset.mesh_artifact.decode", "asset.mesh_artifact.version_mismatch", "Mesh artifact kind or version is unsupported");
		}

		AssetBinaryReader reader(artifact.Payload);
		std::string name;
		Rendering::MeshData data;
		uint32_t layoutElementCount = 0;
		if (!reader.ReadString(name) ||
			!reader.ReadU32(data.Layout.Stride) || data.Layout.Stride == 0 ||
			!reader.ReadU32(layoutElementCount) || layoutElementCount == 0 || layoutElementCount > MaxLayoutElementCount) {
			return MakeMeshArtifactFailure("asset.mesh_artifact.decode", "asset.mesh_artifact.header_invalid", "Mesh artifact header is invalid");
		}

		data.Layout.Elements.reserve(layoutElementCount);
		for (uint32_t index = 0; index < layoutElementCount; ++index) {
			uint8_t type = 0;
			std::string elementName;
			uint32_t size = 0;
			uint32_t offset = 0;
			uint8_t normalized = 0;
			if (!reader.ReadU8(type) || !IsValidShaderDataType(type) ||
				!reader.ReadString(elementName) || elementName.empty() ||
				!reader.ReadU32(size) || size == 0 ||
				!reader.ReadU32(offset) ||
				!reader.ReadU8(normalized) || normalized > 1) {
				return MakeMeshArtifactFailure("asset.mesh_artifact.decode", "asset.mesh_artifact.layout_invalid", "Mesh artifact layout is invalid");
			}
			data.Layout.Elements.emplace_back(type, elementName, size, offset, normalized != 0);
		}

		uint32_t vertexFloatCount = 0;
		if (!reader.ReadU32(vertexFloatCount) || vertexFloatCount == 0 || vertexFloatCount > MaxVertexFloatCount) {
			return MakeMeshArtifactFailure("asset.mesh_artifact.decode", "asset.mesh_artifact.vertex_data_invalid", "Mesh artifact vertex data length is invalid");
		}
		data.VertexData.resize(vertexFloatCount);
		for (float& value : data.VertexData) {
			if (!reader.ReadFloat(value)) {
				return MakeMeshArtifactFailure("asset.mesh_artifact.decode", "asset.mesh_artifact.vertex_data_invalid", "Mesh artifact vertex data is truncated");
			}
		}

		uint32_t indexCount = 0;
		if (!reader.ReadU32(indexCount) || indexCount == 0 || indexCount > MaxIndexCount) {
			return MakeMeshArtifactFailure("asset.mesh_artifact.decode", "asset.mesh_artifact.index_data_invalid", "Mesh artifact index data length is invalid");
		}
		data.IndexData.resize(indexCount);
		for (uint32_t& value : data.IndexData) {
			if (!reader.ReadU32(value)) {
				return MakeMeshArtifactFailure("asset.mesh_artifact.decode", "asset.mesh_artifact.index_data_invalid", "Mesh artifact index data is truncated");
			}
		}

		if (reader.Failed() || reader.Remaining() != 0 || !IsMeshDataConsistent(data)) {
			return MakeMeshArtifactFailure("asset.mesh_artifact.decode", "asset.mesh_artifact.payload_invalid", "Mesh artifact payload is inconsistent");
		}

		outMesh = CreateRef<Rendering::Mesh>(name, data);
		return ResultEnvelope::Success("asset.mesh_artifact.decode", name, "Mesh artifact decoded");
	}
}
