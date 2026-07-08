#pragma once

#include "HuaEngine/Application.h"
#include "HuaEngine/Application/ApplicationOperations.h"
#include "HuaEngine/Application/OperationRegistry.h"
#include "HuaEngine/Automation/AgentHostAdapter.h"
#include "HuaEngine/Core/Log.h"
#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/Core/Layer.h"
#include "HuaEngine/Core/Input.h"
#include "HuaEngine/Asset/AssetRegistry.h"
#include "HuaEngine/Project/ProjectContext.h"

// UI
#include "HuaEngine/GUI/ImguiLayer.h"
#include "imgui.h"

// Render
#include "HuaEngine/Rendering/Renderer.h"
#include "HuaEngine/Rendering/VertexLayout.h"
#include "HuaEngine/Rendering/Texture.h"
#include "HuaEngine/Rendering/FrameBuffer.h"
#include "HuaEngine/Rendering/RHI/TextureResource.h"
#include "HuaEngine/Rendering/Material/Material.h"
#include "HuaEngine/Rendering/Material/MaterialTypes.h"
#include "HuaEngine/Rendering/Material/MaterialLibrary.h"
#include "HuaEngine/Rendering/Camera.h"
#include "HuaEngine/Rendering/EditorCamera.h"

// ECS
#include "HuaEngine/ECS/CommandBuffer.h"
#include "HuaEngine/ECS/ComponentRegistry.h"
#include "HuaEngine/ECS/ComponentType.h"
#include "HuaEngine/ECS/EntityId.h"
#include "HuaEngine/ECS/Entity.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/ECS/Query.h"
#include "HuaEngine/ECS/Scheduler.h"
#include "HuaEngine/ECS/System.h"
#include "HuaEngine/ECS/World.h"
#include "Module/Rendering/RenderingComponent.h"

// Scene
#include "HuaEngine/Scene/Scene.h"

// Reflection
#include "HuaEngine/Reflection/Reflection.h"

// Serialization
#include "HuaEngine/Serialization/SerializationCore.h"
#include "HuaEngine/Serialization/SerializationManager.h"
#include "HuaEngine/Scene/SceneSerializer.h"
#include "HuaEngine/Rendering/Mesh/MeshCore.h"
#include "HuaEngine/Rendering/Material/MaterialSerializer.h"

// Math
#include "HuaEngine/Math/Math.h"

// Rendering type aliases for external code convenience
namespace HE {
	// Core rendering type aliases
	using Camera = Rendering::Camera;
	using Renderer = Rendering::Renderer;

	// Vertex layout related
	using BufferLayout = Rendering::BufferLayout;
	using ShaderDataType = Rendering::ShaderDataType;

	// Shader and texture
	using Texture = Rendering::Texture;
	using TextureResource = Rendering::TextureResource;

	// Framebuffer
	using FrameBuffer = Rendering::FrameBuffer;
	using FrameBufferSpecification = Rendering::FrameBufferSpecification;
	using FrameBufferTextureFormat = Rendering::FrameBufferTextureFormat;

	// Material system
	using Material = Rendering::Material;
	using MaterialInstance = Rendering::MaterialInstance;
	using MaterialType = Rendering::MaterialType;

	// Mesh system
	using Mesh = Rendering::Mesh;
	using MeshData = Rendering::MeshData;
	using MeshManager = Rendering::MeshManager;

	// Serialization system
	using SerializationFormat = Serialization::SerializationFormat;
	using SerializationBackend = Serialization::SerializationBackend;
	using SerializationManager = Serialization::SerializationManager;

	// Rendering component aliases
	using CameraComponent = Rendering::CameraComponent;
	using MaterialComponent = Rendering::MaterialComponent;
	using MeshComponent = Rendering::MeshComponent;
	using RendererComponent = Rendering::RendererComponent;
}
