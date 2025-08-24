#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Scene/Scene.h"
#include "HuaEngine/Rendering/Camera.h"

namespace HE::Rendering {
	class RenderPipeline {
	public:
		virtual ~RenderPipeline();
		virtual void Render(Scene& scene, Camera& camera, float width, float height);
	};
}