#pragma once

#include <variant>
#include <vector>

#include "HuaEngine/Asset/AssetTypes.h"
#include "HuaEngine/ECS/EntityId.h"

namespace HE::Editor {
	struct NoEditorSelection {};

	struct EntitySelection {
		std::vector<EntityUuid> Entities;
	};

	struct AssetSelection {
		AssetGuid Guid;
	};

	using EditorSelection = std::variant<NoEditorSelection, EntitySelection, AssetSelection>;
}
