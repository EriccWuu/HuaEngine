#include "enginepch.h"
#include "ObjMeshImporter.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <unordered_map>
#include <set>
#include <stdexcept>

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
		int NormalIndex = -1;

		bool operator==(const ObjVertexKey&) const = default;
	};

	struct ObjVertexKeyHash {
		size_t operator()(const ObjVertexKey& key) const noexcept {
			size_t seed = std::hash<int>{}(key.PositionIndex);
			seed ^= std::hash<int>{}(key.TexCoordIndex) + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
			seed ^= std::hash<int>{}(key.NormalIndex) + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
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
	namespace {
		std::string_view AxisName(MeshAxis axis) {
			switch (axis) {
			case MeshAxis::PositiveX: return "x"; case MeshAxis::NegativeX: return "-x";
			case MeshAxis::PositiveY: return "y"; case MeshAxis::NegativeY: return "-y";
			case MeshAxis::PositiveZ: return "z"; case MeshAxis::NegativeZ: return "-z";
			}
			return {};
		}
		bool ParseAxis(std::string_view value, MeshAxis& axis) {
			for (const auto candidate : { MeshAxis::PositiveX, MeshAxis::NegativeX, MeshAxis::PositiveY, MeshAxis::NegativeY, MeshAxis::PositiveZ, MeshAxis::NegativeZ })
				if (AxisName(candidate) == value) { axis = candidate; return true; }
			return false;
		}
		bool ParseBool(std::string_view value, bool& output) {
			if (value == "true") { output = true; return true; }
			if (value == "false") { output = false; return true; }
			return false;
		}
		bool SameAxisLine(MeshAxis left, MeshAxis right) { return static_cast<int>(left) / 2 == static_cast<int>(right) / 2; }
		glm::vec3 AxisVector(MeshAxis axis) {
			switch (axis) { case MeshAxis::PositiveX: return { 1, 0, 0 }; case MeshAxis::NegativeX: return { -1, 0, 0 }; case MeshAxis::PositiveY: return { 0, 1, 0 }; case MeshAxis::NegativeY: return { 0, -1, 0 }; case MeshAxis::PositiveZ: return { 0, 0, 1 }; case MeshAxis::NegativeZ: return { 0, 0, -1 }; }
			return {};
		}
		glm::vec3 ConvertVector(const glm::vec3& value, const ObjMeshImportSettings& settings) {
			const auto up = AxisVector(settings.UpAxis); const auto forward = AxisVector(settings.ForwardAxis); const auto right = glm::cross(forward, up);
			return { glm::dot(value, right), glm::dot(value, up), -glm::dot(value, forward) };
		}
	}

	bool ObjMeshImporter::CanImport(AssetKind kind, std::string_view extension) const {
		return kind == AssetKind::Mesh && extension == ".obj";
	}

	std::unique_ptr<AssetImportSettings> ObjMeshImporter::CreateDefaultSettings() const { return std::make_unique<ObjMeshImportSettings>(); }

	ResultEnvelope ObjMeshImporter::DecodeSettings(const AssetMetaSettingsNode& source, std::unique_ptr<AssetImportSettings>& output) const {
		if (source.Values.empty()) { output = CreateDefaultSettings(); return ResultEnvelope::Success("asset.import.settings.decode", std::string(GetId()), "Default OBJ import settings decoded"); }
		static const std::set<std::string> keys = { "flip_uv_v", "forward_axis", "generate_normals_when_missing", "import_scale", "recalculate_normals", "reverse_winding", "up_axis" };
		if (source.Values.size() != keys.size()) return ResultEnvelope::Failure("asset.import.settings.decode", std::string(GetId()), "OBJ import settings are incomplete");
		for (const auto& [key, value] : source.Values) if (!keys.contains(key)) return ResultEnvelope::Failure("asset.import.settings.decode", std::string(GetId()), "OBJ import settings contain an unknown field");
		auto settings = std::make_unique<ObjMeshImportSettings>();
		try { size_t consumed = 0; const auto& value = source.Values.at("import_scale"); settings->ImportScale = std::stof(value, &consumed); if (consumed != value.size()) throw std::invalid_argument("scale"); } catch (...) { return ResultEnvelope::Failure("asset.import.settings.decode", std::string(GetId()), "OBJ import scale is invalid"); }
		if (!ParseAxis(source.Values.at("up_axis"), settings->UpAxis) || !ParseAxis(source.Values.at("forward_axis"), settings->ForwardAxis) ||
			!ParseBool(source.Values.at("flip_uv_v"), settings->FlipUvV) || !ParseBool(source.Values.at("generate_normals_when_missing"), settings->GenerateNormalsWhenMissing) ||
			!ParseBool(source.Values.at("recalculate_normals"), settings->RecalculateNormals) || !ParseBool(source.Values.at("reverse_winding"), settings->ReverseWinding))
			return ResultEnvelope::Failure("asset.import.settings.decode", std::string(GetId()), "OBJ import settings contain an invalid value");
		auto result = ValidateSettings(*settings); if (result.Succeeded()) output = std::move(settings); return result;
	}

	ResultEnvelope ObjMeshImporter::EncodeSettings(const AssetImportSettings& source, AssetMetaSettingsNode& output) const {
		const auto* settings = dynamic_cast<const ObjMeshImportSettings*>(&source);
		if (!settings) return ResultEnvelope::Failure("asset.import.settings.encode", std::string(GetId()), "OBJ import settings type is invalid");
		auto result = ValidateSettings(*settings); if (!result.Succeeded()) return result;
		output.Values = { { "flip_uv_v", settings->FlipUvV ? "true" : "false" }, { "forward_axis", std::string(AxisName(settings->ForwardAxis)) },
			{ "generate_normals_when_missing", settings->GenerateNormalsWhenMissing ? "true" : "false" }, { "import_scale", std::to_string(settings->ImportScale) },
			{ "recalculate_normals", settings->RecalculateNormals ? "true" : "false" }, { "reverse_winding", settings->ReverseWinding ? "true" : "false" }, { "up_axis", std::string(AxisName(settings->UpAxis)) } };
		return ResultEnvelope::Success("asset.import.settings.encode", std::string(GetId()), "OBJ import settings encoded");
	}

	ResultEnvelope ObjMeshImporter::ValidateSettings(const AssetImportSettings& source) const {
		const auto* settings = dynamic_cast<const ObjMeshImportSettings*>(&source);
		if (!settings || !std::isfinite(settings->ImportScale) || settings->ImportScale <= 0.0f || static_cast<int>(settings->UpAxis) % 2 != 0 || SameAxisLine(settings->UpAxis, settings->ForwardAxis))
			return ResultEnvelope::Failure("asset.import.settings.validate", std::string(GetId()), "OBJ import settings are invalid");
		return ResultEnvelope::Success("asset.import.settings.validate", std::string(GetId()), "OBJ import settings are valid");
	}

	AssetImportResult ObjMeshImporter::Import(const AssetImportContext& context) const {
		AssetImportResult result;
		const auto* settings = dynamic_cast<const ObjMeshImportSettings*>(context.Settings);
		if (!settings || !ValidateSettings(*settings).Succeeded()) {
			AddObjDiagnostic(result, "asset.import.obj_settings_invalid", "OBJ import settings are unavailable or invalid", context.SourcePath);
			return result;
		}
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
		const bool includeNormals = settings->RecalculateNormals || settings->GenerateNormalsWhenMissing || !attributes.normals.empty();
		meshData.Layout.Elements = {
			Rendering::SerializableBufferElement{ static_cast<uint8_t>(Rendering::ShaderDataType::Float3), "a_Position", 12, 0, false },
			Rendering::SerializableBufferElement{ static_cast<uint8_t>(Rendering::ShaderDataType::Float2), "a_TexCoord", 8, 12, false }
		};
		if (includeNormals) meshData.Layout.Elements.emplace_back(static_cast<uint8_t>(Rendering::ShaderDataType::Float3), "a_Normal", 12, 20, false);
		meshData.Layout.Stride = includeNormals ? 32 : 20;

		std::unordered_map<ObjVertexKey, uint32_t, ObjVertexKeyHash> vertexLookup;
		bool needsGeneratedNormals = settings->RecalculateNormals || attributes.normals.empty();
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

				const ObjVertexKey key{ sourceIndex.vertex_index, sourceIndex.texcoord_index, settings->RecalculateNormals ? -1 : sourceIndex.normal_index };
				const auto existing = vertexLookup.find(key);
				if (existing != vertexLookup.end()) {
					meshData.IndexData.push_back(existing->second);
					continue;
				}

				const auto position = ConvertVector({ attributes.vertices[positionOffset], attributes.vertices[positionOffset + 1], attributes.vertices[positionOffset + 2] }, *settings) * settings->ImportScale;
				const float x = position.x, y = position.y, z = position.z;
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
				if (settings->FlipUvV) v = 1.0f - v;
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
				if (includeNormals) {
					glm::vec3 normal{};
					if (sourceIndex.normal_index < 0 && settings->GenerateNormalsWhenMissing) needsGeneratedNormals = true;
					if (!settings->RecalculateNormals && sourceIndex.normal_index >= 0) { const size_t offset = static_cast<size_t>(sourceIndex.normal_index) * 3; if (offset + 2 < attributes.normals.size()) normal = glm::normalize(ConvertVector({ attributes.normals[offset], attributes.normals[offset + 1], attributes.normals[offset + 2] }, *settings)); }
					meshData.VertexData.insert(meshData.VertexData.end(), { normal.x, normal.y, normal.z });
				}
				meshData.IndexData.push_back(vertexIndex);
			}
		}

		if (meshData.IndexData.empty() || meshData.IndexData.size() % 3 != 0) {
			AddObjDiagnostic(result, "asset.import.obj_geometry_missing", "OBJ source contains no triangle faces", context.SourcePath);
			return result;
		}
		if (settings->ReverseWinding) {
			for (size_t index = 0; index < meshData.IndexData.size(); index += 3) std::swap(meshData.IndexData[index + 1], meshData.IndexData[index + 2]);
		}
		if (includeNormals && needsGeneratedNormals) {
			const size_t stride = meshData.Layout.Stride / sizeof(float);
			for (size_t vertex = 0; vertex < meshData.VertexData.size(); vertex += stride) meshData.VertexData[vertex + 5] = meshData.VertexData[vertex + 6] = meshData.VertexData[vertex + 7] = 0.0f;
			for (size_t triangle = 0; triangle < meshData.IndexData.size(); triangle += 3) {
				const auto a = meshData.IndexData[triangle], b = meshData.IndexData[triangle + 1], c = meshData.IndexData[triangle + 2];
				const glm::vec3 p0(meshData.VertexData[a * stride], meshData.VertexData[a * stride + 1], meshData.VertexData[a * stride + 2]);
				const glm::vec3 p1(meshData.VertexData[b * stride], meshData.VertexData[b * stride + 1], meshData.VertexData[b * stride + 2]);
				const glm::vec3 p2(meshData.VertexData[c * stride], meshData.VertexData[c * stride + 1], meshData.VertexData[c * stride + 2]);
				const auto normal = glm::cross(p1 - p0, p2 - p0);
				for (const auto vertex : { a, b, c }) for (size_t axis = 0; axis < 3; ++axis) meshData.VertexData[vertex * stride + 5 + axis] += normal[axis];
			}
			for (size_t vertex = 0; vertex < meshData.VertexData.size(); vertex += stride) { auto normal = glm::normalize(glm::vec3(meshData.VertexData[vertex + 5], meshData.VertexData[vertex + 6], meshData.VertexData[vertex + 7])); meshData.VertexData[vertex + 5] = normal.x; meshData.VertexData[vertex + 6] = normal.y; meshData.VertexData[vertex + 7] = normal.z; }
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
