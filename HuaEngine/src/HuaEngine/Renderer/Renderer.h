#pragma once

#include "RendererAPI.h"

namespace HE {

	class Renderer {
	public:
		static void Init();
		static void Begin();
		static void End();
		static void Submit(const Ref<VertexArray>& vaertexArray);
	};
}