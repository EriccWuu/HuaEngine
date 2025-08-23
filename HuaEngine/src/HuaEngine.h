#pragma once

#include "HuaEngine/Application.h"
#include "HuaEngine/Core/Log.h"
#include "HuaEngine/Core/Layer.h"
#include "HuaEngine/Core/Input.h"

// UI
#include "HuaEngine/GUI/ImguiLayer.h"
#include "imgui.h"

// Render
#include "HuaEngine/Rendering/Renderer.h"
#include "HuaEngine/Rendering/RenderCommand.h"
#include "HuaEngine/Rendering/VertexBuffer.h"
#include "HuaEngine/Rendering/IndexBuffer.h"
#include "HuaEngine/Rendering/VertexArray.h"
#include "HuaEngine/Rendering/Shader/Shader.h"
#include "HuaEngine/Rendering/Texture.h"
#include "HuaEngine/Rendering/FrameBuffer.h"
#include "HuaEngine/Rendering/Material/Material.h"
#include "HuaEngine/Rendering/Material/MaterialTypes.h"
#include "HuaEngine/Rendering/Material/MaterialLibrary.h"
#include "HuaEngine/Rendering/Camera.h"
#include "HuaEngine/Rendering/EditorCamera.h"

// ECS
#include "HuaEngine/ECS/Entity.h"
#include "HuaEngine/ECS/EntityManager.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/ECS/ScriptableEntity.h"
#include "Module/Rendering/RenderSystem.h"
#include "Module/Rendering/RenderingComponent.h"

// Reflection
#include "HuaEngine/Reflection/Reflection.h"

// Serialization
#include "HuaEngine/Serialization/SerializationCore.h"

// Math
#include "HuaEngine/Math/Math.h"


