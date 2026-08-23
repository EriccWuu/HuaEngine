#include "enginepch.h"
#include "ObjMeshImporter.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <unordered_map>

#define TINYOBJLOADER_DISABLE_FAST_FLOAT
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

namespace {
	constexpr uintmax_t MaxObjSourceBytes = 512ull * 1024ull * 1024ull;
	constexpr size_t MaxObjVertexCount = 16ull * 1024ull * 1024ull;
	constexpr size_t MaxObjIndexCount = 48ull * 1024ull * 1024ull;

	struct ObjVertexKey {
		int PositionIndex = -1;
		int TexCoordIndex = -1;

		bool operator==(const ObjVertexKey&) const = default;
	};

	struct ObjVertexKeyHash {
		size_t operator()(const ObjVertexKey& key) const noexcept {
			size_t seed = std::hash<int>{}(key.PositionIndex);
			seed ^= std::hash<int>{}(key.TexCoordIndex) + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
			return seed;
		}
	};

	void AddObjDiagnostic(
		HE::AssetImportResult& result,
		std::string code,
		std::string message,
		const std::filesystem::path& sourcePath) {
		result.Diagnostics.push_back({
			HE::DiagnosticSeverity::Error,
			std::move(code),
			std::move(message),
			sourcePath.generic_string()
		});
	}

	bool IsFinite(float value) {
		return std::isfinite(value);
	}
}

namespace HE {
	bool ObjMeshImporter::CanImport(AssetKind kind, std::string_view extension) const {
		return kind == AssetKind::Mesh && extension == ".obj";
	}

	AssetImportResult ObjMeshImporter::Import(const AssetImportContext& context) const {
		AssetImportResult result;
		std::error_code errorCode;
		const auto sourceBytes = std::filesystem::file_size(context.SourcePath, errorCode);
		if (errorCode || sourceBytes == 0 || sourceBytes > MaxObjSourceBytes) {
			AddObjDiagnostic(result, "asset.import.obj_source_invalid", "OBJ source file is empty, unavailable, or exceeds the import size limit", context.SourcePath);
			return result;
		}

		std::ifstream source(context.SourcePath, std::ios::in | std::ios::binary);
		if (!source.is_open()) {
			AddObjDiagnostic(result, "asset.import.obj_source_unreadable", "OBJ source file could not be opened", context.SourcePath);
			return result;
		}
		std::string sourceText((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
		if (!source.good() && !source.eof()) {
			AddObjDiagnostic(result, "asset.import.obj_source_unreadable", "OBJ source file could not be read", context.SourcePath);
			return result;
		}

		tinyobj::ObjReaderConfig config;
		config.triangulate = true;
		config.vertex_color = false;
		tinyobj::ObjReader reader;
		if (!reader.ParseFromString(sourceText, {}, config)) {
			const auto message = reader.Error().empty() ? "OBJ source could not be parsed" : reader.Error();
			AddObjDiagnostic(result, "asset.import.obj_parse_failed", message, context.SourcePath);
			return result;
		}

		const auto& attributes = reader.GetAttrib();
		const auto& shapes = reader.GetShapes();
		if (attributes.vertices.empty() || attributes.vertices.size() % 3 != 0 || shapes.empty()) {
			AddObjDiagnostic(result, "asset.import.obj_geometry_missing", "OBJ source contains no valid vertex or face geometry", context.SourcePath);
			return result;
		}

		Rendering::MeshData meshData;
		meshData.Layout.Elements = {
			Rendering::SerializableBufferElement{ static_cast<uint8_t>(Rendering::ShaderDataType::Float3), "a_Position", 12, 0, false },
			Rendering::SerializableBufferElement{ static_cast<uint8_t>(Rendering::ShaderDataType::Float2), "a_TexCoord", 8, 12, false }
		};
		meshData.Layout.Stride = 20;

		std::unordered_map<ObjVertexKey, uint32_t, ObjVertexKeyHash> vertexLookup;
		for (const auto& shape : shapes) {
			for (const auto faceVertexCount : shape.mesh.num_face_vertices) {
				if (faceVertexCount != 3) {
					AddObjDiagnostic(result, "asset.import.obj_triangulation_failed", "OBJ face triangulation did not produce triangles", context.SourcePath);
					return result;
				}
			}

			for (const auto& sourceIndex : shape.mesh.indices) {
				if (meshData.IndexData.size() >= MaxObjIndexCount) {
					AddObjDiagnostic(result, "asset.import.obj_limit_exceeded", "OBJ geometry exceeds importer vertex or index limits", context.SourcePath);
					return result;
				}
				if (sourceIndex.vertex_index < 0) {
					AddObjDiagnostic(result, "asset.import.obj_index_invalid", "OBJ face references a missing position", context.SourcePath);
					return result;
				}

				const auto positionOffset = static_cast<size_t>(sourceIndex.vertex_index) * 3;
				if (positionOffset + 2 >= attributes.vertices.size()) {
					AddObjDiagnostic(result, "asset.import.obj_index_invalid", "OBJ face position index is out of range", context.SourcePath);
					return result;
				}

				const ObjVertexKey key{ sourceIndex.vertex_index, sourceIndex.texcoord_index };
				const auto existing = vertexLookup.find(key);
				if (existing != vertexLookup.end()) {
					meshData.IndexData.push_back(existing->second);
					continue;
				}

				const float x = attributes.vertices[positionOffset];
				const float y = attributes.vertices[positionOffset + 1];
				const float z = attributes.vertices[positionOffset + 2];
				float u = 0.0f;
				float v = 0.0f;
				if (sourceIndex.texcoord_index >= 0) {
					const auto texCoordOffset = static_cast<size_t>(sourceIndex.texcoord_index) * 2;
					if (texCoordOffset + 1 >= attributes.texcoords.size()) {
						AddObjDiagnostic(result, "asset.import.obj_index_invalid", "OBJ face texture coordinate index is out of range", context.SourcePath);
						return result;
					}
					u = attributes.texcoords[texCoordOffset];
					v = attributes.texcoords[texCoordOffset + 1];
				}
				if (!IsFinite(x) || !IsFinite(y) || !IsFinite(z) || !IsFinite(u) || !IsFinite(v)) {
					AddObjDiagnostic(result, "asset.import.obj_value_invalid", "OBJ source contains a non-finite vertex value", context.SourcePath);
					return result;
				}
				if (vertexLookup.size() >= MaxObjVertexCount) {
					AddObjDiagnostic(result, "asset.import.obj_limit_exceeded", "OBJ geometry exceeds importer vertex or index limits", context.SourcePath);
					return result;
				}

				const auto vertexIndex = static_cast<uint32_t>(vertexLookup.size());
				vertexLookup.emplace(key, vertexIndex);
				meshData.VertexData.insert(meshData.VertexData.end(), { x, y, z, u, v });
				meshData.IndexData.push_back(vertexIndex);
			}
		}

		if (meshData.IndexData.empty() || meshData.IndexData.size() % 3 != 0) {
			AddObjDiagnostic(result, "asset.import.obj_geometry_missing", "OBJ source contains no triangle faces", context.SourcePath);
			return result;
		}

		const auto meshName = context.SourcePath.stem().string();
		const Rendering::Mesh mesh(meshName, meshData);
		auto encodeResult = EncodeMeshArtifact(mesh, result.Artifact);
		if (!encodeResult.Succeeded()) {
			result.Diagnostics = std::move(encodeResult.Details);
			return result;
		}

		result.Success = true;
		return result;
	}
}
