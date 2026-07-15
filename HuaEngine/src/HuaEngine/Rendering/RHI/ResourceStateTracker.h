#pragma once

#include <unordered_map>

#include "HuaEngine/Rendering/RHI/ResourceBarrier.h"
#include "HuaEngine/Rendering/RHI/TextureResource.h"

namespace HE::Rendering {
	class ResourceStateTracker {
	public:
		bool Transition(const Ref<TextureResource>& texture, ResourceState after, ResourceBarrier& outBarrier) {
			if (!texture) {
				return false;
			}

			const auto* key = texture.get();
			const auto found = m_TextureStates.find(key);
			const auto before = found == m_TextureStates.end() ? ResourceState::Undefined : found->second;
			if (before == after) {
				return false;
			}

			outBarrier = {
				.Texture = texture,
				.Before = before,
				.After = after
			};
			m_TextureStates[key] = after;
			return true;
		}

		[[nodiscard]] ResourceState GetState(const Ref<TextureResource>& texture) const {
			if (!texture) {
				return ResourceState::Undefined;
			}

			const auto found = m_TextureStates.find(texture.get());
			return found == m_TextureStates.end() ? ResourceState::Undefined : found->second;
		}

		void Reset() {
			m_TextureStates.clear();
		}

	private:
		std::unordered_map<const TextureResource*, ResourceState> m_TextureStates;
	};
}
