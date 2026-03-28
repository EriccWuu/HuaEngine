#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Scene/Scene.h"
#include "Workbench/EditorWorkbenchState.h"
#include "imgui.h"
#include "glm/glm.hpp"

namespace HE {
	struct TreeNode
	{
		// Tree structure
		std::string Name = "";
		int UID = 0;
		TreeNode* Parent = NULL;
		std::vector<TreeNode*> Childs;
		unsigned short IndexInParent = 0;  // Maintaining this allows us to implement linear traversal more easily

		// Leaf Data
		bool HasData = false;    // All leaves have data
		bool DataMyBool = true;
		int  DataMyInt = 128;
		glm::vec2 DataMyVec2 = glm::vec2(0.0f, 3.141592f);
	};

	class HierarchyPanel {
	public:
		HierarchyPanel() = default;
		HierarchyPanel(const Ref<Scene>& scene);
		~HierarchyPanel() = default;

		void OnGuiRender();

		void SetContext(const Ref<Scene>& scene);
		void SetWorkbenchState(const EditorWorkbenchState* state) { m_WorkbenchState = state; }

	private:
		void DrawEntityNode(Entity& eneity);

	private:
		Ref<Scene> m_Context;
        const EditorWorkbenchState* m_WorkbenchState = nullptr;

		ImGuiTextFilter m_Filter;
		TreeNode* m_RootNode = NULL;
	};
}
