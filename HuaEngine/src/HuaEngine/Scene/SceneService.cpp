#include "enginepch.h"
#include "SceneService.h"

#include <system_error>

#include "HuaEngine/Serialization/Serialization.h"
#include "Module/Rendering/RenderingComponent.h"

namespace {
	std::filesystem::path NormalizePath(const std::filesystem::path& path) {
		if (path.empty()) {
			return {};
		}

		std::error_code errorCode;
		auto absolutePath = std::filesystem::absolute(path, errorCode);
		if (errorCode) {
			return path.lexically_normal();
		}

		if (std::filesystem::exists(absolutePath, errorCode)) {
			auto canonicalPath = std::filesystem::weakly_canonical(absolutePath, errorCode);
			if (!errorCode) {
				return canonicalPath;
			}
		}

		return absolutePath.lexically_normal();
	}

	std::string CountToString(uint32_t value) {
		return std::to_string(value);
	}
}

namespace HE {
	bool SceneValidationReport::IsOperational() const {
		return HasName &&
			EntitiesMissingTransform == 0 &&
			RenderEntitiesMissingMaterial == 0 &&
			RenderEntitiesMissingMesh == 0 &&
			EntitiesUsingLegacyRenderer == 0;
	}

	bool SceneValidationReport::HasIssues() const {
		return !IsOperational();
	}

	ResultEnvelope SceneService::CreateScene(std::string_view sceneName, Ref<Scene>& outScene) const {
		auto resolvedName = sceneName.empty() ? std::string("Untitled Scene") : std::string(sceneName);
		outScene = CreateRef<Scene>(resolvedName);

		auto result = ResultEnvelope::Success("scene.create", resolvedName, "Scene created");
		result.SetPayloadValue("scene_name", resolvedName);
		result.SetPayloadValue("entity_count", "0");
		return result;
	}

	ResultEnvelope SceneService::LoadScene(const std::filesystem::path& scenePath, Ref<Scene>& outScene) const {
		auto normalizedPath = NormalizePath(scenePath);
		auto targetId = normalizedPath.generic_string();
		std::error_code errorCode;

		if (normalizedPath.empty()) {
			auto result = ResultEnvelope::Failure("scene.load", targetId, "Scene path is empty");
			result.AddDetail({ DiagnosticSeverity::Error, "scene.path.empty", "Cannot load a scene from an empty path", {} });
			return result;
		}

		if (!std::filesystem::exists(normalizedPath, errorCode)) {
			auto result = ResultEnvelope::Failure("scene.load", targetId, "Scene file does not exist");
			result.AddDetail({ DiagnosticSeverity::Error, "scene.file.missing", "Scene file was not found", normalizedPath.generic_string() });
			return result;
		}

		if (!std::filesystem::is_regular_file(normalizedPath, errorCode)) {
			auto result = ResultEnvelope::Failure("scene.load", targetId, "Scene path is not a regular file");
			result.AddDetail({ DiagnosticSeverity::Error, "scene.file.invalid_type", "Scene path must point to a regular file", normalizedPath.generic_string() });
			return result;
		}

		auto loadedScene = CreateRef<Scene>();
		if (!Serialization::LoadScene(normalizedPath.string(), *loadedScene)) {
			auto result = ResultEnvelope::ManualIntervention("scene.load", targetId, "Scene file exists but could not be deserialized");
			result.AddDetail({ DiagnosticSeverity::Error, "scene.deserialize.failed", "Scene deserialization returned false", normalizedPath.generic_string() });
			return result;
		}

		outScene = loadedScene;

		auto result = ResultEnvelope::Success("scene.load", targetId, "Scene loaded");
		result.SetPayloadValue("scene_name", loadedScene->GetName());
		result.SetPayloadValue("scene_path", normalizedPath.generic_string());
		return result;
	}

	ResultEnvelope SceneService::SaveScene(const Scene& scene, const std::filesystem::path& scenePath) const {
		auto normalizedPath = NormalizePath(scenePath);
		auto targetId = normalizedPath.generic_string();

		if (normalizedPath.empty()) {
			auto result = ResultEnvelope::Failure("scene.save", targetId, "Scene path is empty");
			result.AddDetail({ DiagnosticSeverity::Error, "scene.path.empty", "Cannot save a scene to an empty path", {} });
			return result;
		}

		auto parentPath = normalizedPath.parent_path();
		if (!parentPath.empty()) {
			std::error_code errorCode;
			std::filesystem::create_directories(parentPath, errorCode);
			if (errorCode) {
				auto result = ResultEnvelope::Failure("scene.save", targetId, "Failed to create scene directory");
				result.AddDetail({ DiagnosticSeverity::Error, "scene.directory.create_failed", errorCode.message(), parentPath.generic_string() });
				return result;
			}
		}

		if (!Serialization::SaveScene(const_cast<Scene&>(scene), normalizedPath.string())) {
			auto result = ResultEnvelope::Failure("scene.save", targetId, "Scene serialization failed");
			result.AddDetail({ DiagnosticSeverity::Error, "scene.serialize.failed", "Scene serializer returned false", normalizedPath.generic_string() });
			return result;
		}

		auto result = ResultEnvelope::Success("scene.save", targetId, "Scene saved");
		result.SetPayloadValue("scene_name", scene.GetName());
		result.SetPayloadValue("scene_path", normalizedPath.generic_string());
		return result;
	}

	ResultEnvelope SceneService::ValidateScene(const Scene& scene, SceneValidationReport* outReport) const {
		SceneValidationReport report;
		report.HasName = !scene.GetName().empty();

		const_cast<Scene&>(scene).GetWorld().ForEachEntity([&](Entity entity) {
			++report.EntityCount;

			const bool hasTransform = entity.HasComponent<TransformComponent>();
			const bool hasMesh = entity.HasComponent<Rendering::MeshComponent>();
			const bool hasMaterial = entity.HasComponent<Rendering::MaterialComponent>();
			const bool hasLegacyRenderer = entity.HasComponent<Rendering::RendererComponent>();

			if (!hasTransform) {
				++report.EntitiesMissingTransform;
			}

			if (hasMesh && !hasMaterial) {
				++report.RenderEntitiesMissingMaterial;
			}

			if (!hasMesh && hasMaterial) {
				++report.RenderEntitiesMissingMesh;
			}

			if (hasLegacyRenderer) {
				++report.EntitiesUsingLegacyRenderer;
			}
		});

		if (outReport) {
			*outReport = report;
		}

		auto targetId = scene.GetName().empty() ? std::string("<unnamed-scene>") : scene.GetName();
		auto result = report.IsOperational()
			? ResultEnvelope::Success("scene.validate", targetId, "Scene is structurally valid")
			: ResultEnvelope::ManualIntervention("scene.validate", targetId, "Scene requires structural fixes");

		result.SetPayloadValue("scene_name", scene.GetName());
		result.SetPayloadValue("entity_count", CountToString(report.EntityCount));
		result.SetPayloadValue("missing_transform_count", CountToString(report.EntitiesMissingTransform));
		result.SetPayloadValue("render_entities_missing_material", CountToString(report.RenderEntitiesMissingMaterial));
		result.SetPayloadValue("render_entities_missing_mesh", CountToString(report.RenderEntitiesMissingMesh));
		result.SetPayloadValue("legacy_renderer_entity_count", CountToString(report.EntitiesUsingLegacyRenderer));

		if (!report.HasName) {
			result.AddDetail({ DiagnosticSeverity::Error, "scene.name.empty", "Scene name is empty", {} });
		}
		if (report.EntitiesMissingTransform > 0) {
			result.AddDetail({ DiagnosticSeverity::Error, "scene.entities.missing_transform", "One or more entities are missing TransformComponent", CountToString(report.EntitiesMissingTransform) });
		}
		if (report.RenderEntitiesMissingMaterial > 0) {
			result.AddDetail({ DiagnosticSeverity::Warning, "scene.render_entities.missing_material", "One or more mesh entities are missing MaterialComponent", CountToString(report.RenderEntitiesMissingMaterial) });
		}
		if (report.RenderEntitiesMissingMesh > 0) {
			result.AddDetail({ DiagnosticSeverity::Warning, "scene.render_entities.missing_mesh", "One or more render entities are missing MeshComponent", CountToString(report.RenderEntitiesMissingMesh) });
		}
		if (report.EntitiesUsingLegacyRenderer > 0) {
			result.AddDetail({ DiagnosticSeverity::Error, "scene.render_entities.legacy_renderer", "Legacy RendererComponent is not part of the formal SceneService/SceneSerializer scene contract", CountToString(report.EntitiesUsingLegacyRenderer) });
		}

		return result;
	}
}
