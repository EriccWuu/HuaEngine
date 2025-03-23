#pragma once

#include "RendererAPI.h"

namespace HE {

	class Renderer {
	public:
		static void Begin();
		static void End();
		static void Submit(const std::shared_ptr<VertexArray>& vaertexArray);
	};
}