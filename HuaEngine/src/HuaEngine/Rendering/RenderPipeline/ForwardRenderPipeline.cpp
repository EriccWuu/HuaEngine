#include "enginepch.h"
#include "ForwardRenderPipeline.h"

#include "HuaEngine/Rendering/RenderCommand.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.h"
#include "HuaEngine/Rendering/Renderer.h"

namespace HE::Rendering {
	void ForwardOpaquePass::Execute(RenderPassContext& context) {
		if (!context.RenderItems || !context.ResourceResolver || !context.Stats || !context.Diagnostics) {
			return;
		}

		++context.Stats->PassCount;

		for (const auto& item : *context.RenderItems) {
			ResolvedRenderItem resolvedItem;
			if (!context.ResourceResolver->Resolve(item, resolvedItem, *context.Stats, *context.Diagnostics)) {
				++context.Stats->SkippedItems;
				continue;
			}

			Renderer::Submit(resolvedItem.MaterialInstanceRef, resolvedItem.VertexArrayRef, item.Transform);
			++context.Stats->SubmittedItems;
			++context.Stats->DrawCalls;
		}
	}

	RenderResult ForwardRenderPipeline::Render(
		const RenderView& view,
		const std::vector<RenderItem>& renderItems,
		const RenderResourceResolver& resourceResolver) {
		RenderResult result;
		result.Stats.RenderItems = static_cast<uint32_t>(renderItems.size());
		result.Stats.VisibleItems = result.Stats.RenderItems;

		if (!view.CameraRef || !view.Target) {
			return result;
		}

		view.Target->Bind();
		if (view.ClearColorBuffer) {
			RenderCommand::SetClearColor(view.ClearColor);
			RenderCommand::Clear();
		}

		RenderPassContext passContext;
		passContext.View = &view;
		passContext.RenderItems = &renderItems;
		passContext.ResourceResolver = &resourceResolver;
		passContext.Stats = &result.Stats;
		passContext.Diagnostics = &result.Diagnostics;

		Renderer::Begin(view.CameraRef);
		m_OpaquePass.Execute(passContext);
		Renderer::End();
		view.Target->Unbind();

		result.Succeeded = true;
		return result;
	}
}
