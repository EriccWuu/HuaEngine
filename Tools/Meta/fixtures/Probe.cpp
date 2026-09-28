#include "Module/Rendering/RenderingComponent.h"

#define HE_ECS_QUERY(...)

namespace HuaMetaProbe {
    HE_ECS_QUERY()
    void Inspect(HE::Rendering::MaterialComponent& material,
                 const HE::TransformComponent& transform,
                 const HE::Rendering::MeshComponent* optionalMesh,
                 float deltaTime);
}
