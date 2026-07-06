#include "enginepch.h"
#include "VertexArray.h"
#include "Platform/OpenGL/OpenGLVertexArray.h"

namespace HE::Rendering {
	Ref<VertexArray> VertexArray::Create()
	{
		return CreateRef<OpenGLVertexArray>();
	}
}
