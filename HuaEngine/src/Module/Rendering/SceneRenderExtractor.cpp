#include "enginepch.h"
#include "SceneRenderExtractor.h"

#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/ECS/World.h"
#include "Module/Rendering/RenderingComponent.h"

namespace HE {
	std::vector<Rendering::RenderItem> SceneRenderExtractor::Extract(World& world) const {
		std::vector<Rendering::RenderItem> renderItems;

		auto query = world.Query<TransformComponent, Rendering::MeshComponent, Rendering::MaterialComponent>();
		query.ForEach([&](
			Entity entity,
			TransformComponent& transform,
			Rendering::MeshComponent& mesh,
			Rendering::MaterialComponent& material) {
			Rendering::RenderItem item;
			item.SourceEntity = entity;
			item.Transform = transform.GetTransformMat();
			item.Mesh = mesh.Mesh;
			item.Material = material.Material;
			item.MaterialOverrides = material.Overrides;
			renderItems.push_back(item);
		});

		return renderItems;
	}
}
