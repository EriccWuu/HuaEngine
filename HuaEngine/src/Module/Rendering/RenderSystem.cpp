#include "enginepch.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Rendering/RenderPipeline/ForwardRenderPipeline.h"
#include "RenderFrameData.h"
#include "RenderingComponent.h"
#include "RenderSystem.h"

namespace HE {
	RenderSystem::RenderSystem()
		: m_RenderPipeline(CreateScope<Rendering::ForwardRenderPipeline>()) {}

	SystemDescriptor RenderSystem::Describe() const {
		SystemDescriptor descriptor;
		descriptor.Name = "RenderSystem";
		descriptor.Stage = SystemStage::Render;
		descriptor.Accesses = {
			SystemAccess::ReadComponent<TransformComponent>(),
			SystemAccess::ReadComponent<Rendering::MeshComponent>(),
			SystemAccess::ReadComponent<Rendering::MaterialComponent>(),
			SystemAccess::ReadFrameResource(Rendering::RenderFrameData::ResourceName)
		};
		return descriptor;
	}

	void RenderSystem::Update(SystemContext& context) {
		const auto* renderFrame = context.Frame().TryGet<Rendering::RenderFrameData>();
		if (!renderFrame || !renderFrame->ActiveCamera) {
			return;
		}

		RenderSingleCamera(context.WorldRef(), *renderFrame->ActiveCamera);
	}

	void RenderSystem::RenderSingleCamera(World& world, const Rendering::RenderCamera& camera, bool drawEditorGrid) {
		m_LastRenderResult = {};
		if (!m_RenderTarget) {
			HE_CORE_WARN("RenderSystem::RenderSingleCamera skipped because no render target is attached");
			return;
		}

		Rendering::RenderView view;
		view.CameraRef = CreateRef<Rendering::RenderCamera>(camera);
		view.Target = m_RenderTarget;
		view.DrawEditorGrid = drawEditorGrid;

		auto renderItems = m_Extractor.Extract(world);
		m_LastRenderResult = m_RenderPipeline->Render(view, renderItems, m_ResourceResolver);
	}
}
