#pragma once

#include "HuaEngine/Asset/AssetService.h"
#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Project/ProjectService.h"
#include "HuaEngine/Reflection/ReflectionToolService.h"
#include "HuaEngine/Scene/SceneService.h"
#include "HuaEngine/Script/ScriptService.h"
#include "HuaEngine/Validation/ValidationService.h"

namespace HE {
	class ENGINE_API ApplicationServices {
	public:
		ApplicationServices()
			: m_ValidationService(m_ProjectService, m_SceneService) {}

		[[nodiscard]] ProjectService& Projects() { return m_ProjectService; }
		[[nodiscard]] const ProjectService& Projects() const { return m_ProjectService; }

		[[nodiscard]] SceneService& Scenes() { return m_SceneService; }
		[[nodiscard]] const SceneService& Scenes() const { return m_SceneService; }

		[[nodiscard]] AssetService& Assets() { return m_AssetService; }
		[[nodiscard]] const AssetService& Assets() const { return m_AssetService; }

		[[nodiscard]] ScriptService& Scripts() { return m_ScriptService; }
		[[nodiscard]] const ScriptService& Scripts() const { return m_ScriptService; }

		[[nodiscard]] ValidationService& Validation() { return m_ValidationService; }
		[[nodiscard]] const ValidationService& Validation() const { return m_ValidationService; }

		[[nodiscard]] ReflectionToolService& ReflectionTools() { return m_ReflectionToolService; }
		[[nodiscard]] const ReflectionToolService& ReflectionTools() const { return m_ReflectionToolService; }

	private:
		ProjectService m_ProjectService;
		SceneService m_SceneService;
		AssetService m_AssetService;
		ScriptService m_ScriptService;
		ValidationService m_ValidationService;
		ReflectionToolService m_ReflectionToolService;
	};
}
