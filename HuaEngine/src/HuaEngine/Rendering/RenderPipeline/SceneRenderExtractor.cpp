#include "enginepch.h"
#include "SceneRenderExtractor.h"

#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/ECS/World.h"
#include "Module/Rendering/RenderingComponent.h"

namespace HE::Rendering {
	std::vector<RenderItem> SceneRenderExtractor::Extract(World& world) const {
		std::vector<RenderItem> renderItems;

		auto query = world.Query<TransformComponent, MeshComponent, MaterialComponent>();
		query.ForEach([&](Entity entity, TransformComponent& transform, MeshComponent& mesh, MaterialComponent& material) {
			auto vertexArray = mesh.GetVertexArray();

			RenderItem item;
			item.SourceEntity = entity;
			item.Transform = transform.GetTransformMat();
			item.VertexArrayRef = vertexArray;
			item.MaterialInstanceRef = material.MaterialInstance;
			renderItems.push_back(item);
		});

		return renderItems;
	}
}
