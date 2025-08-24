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

// 渲染类型别名，便于外部代码使用
namespace HE {
	// 渲染核心类型别名
	using Camera = Rendering::Camera;
	using Renderer = Rendering::Renderer;
	using RenderCommand = Rendering::RenderCommand;
	using RendererAPI = Rendering::RendererAPI;
	
	// 缓冲区相关
	using Buffer = Rendering::Buffer;
	using VertexBuffer = Rendering::VertexBuffer;
	using IndexBuffer = Rendering::IndexBuffer;
	using VertexArray = Rendering::VertexArray;
	using BufferLayout = Rendering::BufferLayout;
	using ShaderDataType = Rendering::ShaderDataType;
	
	// 着色器和纹理
	using Shader = Rendering::Shader;
	using Texture = Rendering::Texture;
	using Texture2D = Rendering::Texture2D;
	
	// 帧缓冲
	using FrameBuffer = Rendering::FrameBuffer;
	using FrameBufferSpecification = Rendering::FrameBufferSpecification;
	using FrameBufferTextureFormat = Rendering::FrameBufferTextureFormat;
	
	// 材质系统
	using Material = Rendering::Material;
	using MaterialInstance = Rendering::MaterialInstance;
	
	// 网格系统
	using Mesh = Rendering::Mesh;
	using MeshData = Rendering::MeshData;
	using MeshManager = Rendering::MeshManager;
	
	// 渲染组件别名
	using CameraComponent = Rendering::CameraComponent;
	using MaterialComponent = Rendering::MaterialComponent;
	using MeshComponent = Rendering::MeshComponent;
	using RendererComponent = Rendering::RendererComponent;
}


