#include "enginepch.h"
#include "RenderPipeline.h"

#include "HuaEngine/Rendering/RenderCommand.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.h"
#include "HuaEngine/Rendering/Renderer.h"

namespace HE::Rendering {
	RenderPipeline::~RenderPipeline() = default;

	RenderResult RenderPipeline::Render(
		const RenderView& view,
		const std::vector<RenderItem>& renderItems,
		const RenderResourceResolver& resourceResolver) {
		RenderResult result;
		result.Stats.RenderItems = static_cast<uint32_t>(renderItems.size());
		result.Stats.VisibleItems = result.Stats.RenderItems;

		if (!view.CameraRef || !view.Target) {
			return result;
		}

		result.Stats.PassCount = 1;

		view.Target->Bind();
		if (view.ClearColorBuffer) {
			RenderCommand::SetClearColor(view.ClearColor);
			RenderCommand::Clear();
		}

		Renderer::Begin(view.CameraRef);
		for (const auto& item : renderItems) {
			ResolvedRenderItem resolvedItem;
			if (!resourceResolver.Resolve(item, resolvedItem, result.Diagnostics)) {
				++result.Stats.SkippedItems;
				continue;
			}

			Renderer::Submit(resolvedItem.MaterialInstanceRef, resolvedItem.VertexArrayRef, item.Transform);
			++result.Stats.SubmittedItems;
			++result.Stats.DrawCalls;
		}
		Renderer::End();
		view.Target->Unbind();

		result.Succeeded = true;
		return result;
	}
}
