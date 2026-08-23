#pragma once

#include <unordered_map>
#include <utility>

#include "AssetTypes.h"
#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Rendering/Material/Material.h"
#include "HuaEngine/Rendering/Mesh/MeshCore.h"
#include "HuaEngine/Rendering/RHI/TextureResource.h"
#include "HuaEngine/Rendering/RHI/ShaderProgram.h"

namespace HE {
	class AssetRuntimeCache {
	public:
		void StoreMesh(const AssetGuid& guid, Ref<Rendering::Mesh> mesh) { m_Meshes[guid] = std::move(mesh); }
		void StoreMaterial(const AssetGuid& guid, Ref<Rendering::Material> material) { m_Materials[guid] = std::move(material); }
		void StoreTexture(const AssetGuid& guid, Ref<Rendering::TextureResource> texture) { m_Textures[guid] = std::move(texture); }
		void StoreShader(const AssetGuid& guid, Ref<Rendering::ShaderProgram> shader) { m_Shaders[guid] = std::move(shader); }
		void Invalidate(const AssetGuid& guid) {
			m_Meshes.erase(guid);
			m_Materials.erase(guid);
			m_Textures.erase(guid);
			m_Shaders.erase(guid);
		}

		[[nodiscard]] Ref<Rendering::Mesh> FindMesh(const AssetGuid& guid) const {
			const auto it = m_Meshes.find(guid);
			return it != m_Meshes.end() ? it->second : nullptr;
		}

		[[nodiscard]] Ref<Rendering::Material> FindMaterial(const AssetGuid& guid) const {
			const auto it = m_Materials.find(guid);
			return it != m_Materials.end() ? it->second : nullptr;
		}

		[[nodiscard]] Ref<Rendering::TextureResource> FindTexture(const AssetGuid& guid) const {
			const auto it = m_Textures.find(guid);
			return it != m_Textures.end() ? it->second : nullptr;
		}

		[[nodiscard]] Ref<Rendering::ShaderProgram> FindShader(const AssetGuid& guid) const {
			const auto it = m_Shaders.find(guid);
			return it != m_Shaders.end() ? it->second : nullptr;
		}

	private:
		std::unordered_map<AssetGuid, Ref<Rendering::Mesh>> m_Meshes;
		std::unordered_map<AssetGuid, Ref<Rendering::Material>> m_Materials;
		std::unordered_map<AssetGuid, Ref<Rendering::TextureResource>> m_Textures;
		std::unordered_map<AssetGuid, Ref<Rendering::ShaderProgram>> m_Shaders;
	};
}
