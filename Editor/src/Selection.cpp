#include "enginepch.h"
#include "Selection.h"

#include <algorithm>

namespace HE {
	namespace {
		std::vector<Entity>& ResolvedSelectionCache() {
			static std::vector<Entity> selections;
			return selections;
		}

		Entity& EmptySelection() {
			static Entity selection;
			return selection;
		}
	}

	Editor::EditorSelectionService& Selection::GetService() {
		static Editor::EditorSelectionService service;
		return service;
	}

	void Selection::SetSelection(const Entity& selection) {
		SetSelectedEntity(selection.IsValid() ? selection.GetUuid() : EntityUuid{});
	}

	void Selection::SetSelectedEntity(EntityUuid uuid) {
		GetService().SelectEntities(uuid == EntityUuid{} ? std::vector<EntityUuid>{} : std::vector<EntityUuid>{ uuid });
	}

	void Selection::SetSelections(std::vector<Entity> selections) {
		std::vector<EntityUuid> entities;
		entities.reserve(selections.size());
		for (const Entity& selection : selections) {
			if (selection.IsValid()) entities.push_back(selection.GetUuid());
		}
		GetService().SelectEntities(std::move(entities));
	}

	void Selection::SetSelectedEntities(std::vector<EntityUuid> selections) {
		GetService().SelectEntities(std::move(selections));
	}

	void Selection::AddToSelection(const Entity& selection) {
		if (!selection.IsValid() || IsSelected(selection)) return;
		auto entities = GetSelectedEntityUuids();
		entities.push_back(selection.GetUuid());
		GetService().SelectEntities(std::move(entities));
	}

	void Selection::ToggleSelection(const Entity& selection) {
		if (!selection.IsValid()) return;
		if (IsSelected(selection)) RemoveFromSelection(selection); else AddToSelection(selection);
	}

	void Selection::RemoveFromSelection(const Entity& selection) {
		auto entities = GetSelectedEntityUuids();
		const EntityUuid uuid = selection.GetUuid();
		entities.erase(std::remove(entities.begin(), entities.end(), uuid), entities.end());
		GetService().SelectEntities(std::move(entities));
	}

	Entity& Selection::GetSelection() {
		RemoveInvalidSelections();
		return EmptySelection();
	}

	const std::vector<Entity>& Selection::GetSelections() {
		RemoveInvalidSelections();
		auto& selections = ResolvedSelectionCache();
		selections.clear();
		return selections;
	}

	Entity Selection::ResolvePrimarySelection(World& world) {
		RemoveInvalidSelections(world);
		const auto& entities = GetSelectedEntityUuids();
		return entities.empty() ? Entity{} : world.GetEntity(entities.front());
	}

	const std::vector<Entity>& Selection::ResolveSelections(World& world) {
		RemoveInvalidSelections(world);
		auto& selections = ResolvedSelectionCache();
		selections.clear();
		selections.reserve(GetSelectedEntityUuids().size());
		for (EntityUuid uuid : GetSelectedEntityUuids()) {
			Entity entity = world.GetEntity(uuid);
			if (entity.IsValid()) selections.push_back(entity);
		}
		return selections;
	}

	EntityUuid Selection::GetSelectedEntityUuid() {
		RemoveInvalidSelections();
		const auto& entities = GetSelectedEntityUuids();
		return entities.empty() ? EntityUuid{} : entities.front();
	}

	const std::vector<EntityUuid>& Selection::GetSelectedEntityUuids() {
		static const std::vector<EntityUuid> empty;
		const auto* selection = GetService().GetEntitySelection();
		return selection ? selection->Entities : empty;
	}

	bool Selection::HasSelection() {
		RemoveInvalidSelections();
		return GetService().HasEntitySelection();
	}

	bool Selection::HasSingleSelection() {
		RemoveInvalidSelections();
		return GetSelectedEntityUuids().size() == 1;
	}

	bool Selection::IsSelected(const Entity& selection) {
		RemoveInvalidSelections();
		if (!selection.IsValid()) return false;
		const auto& entities = GetSelectedEntityUuids();
		return std::find(entities.begin(), entities.end(), selection.GetUuid()) != entities.end();
	}

	size_t Selection::Count() {
		RemoveInvalidSelections();
		return GetSelectedEntityUuids().size();
	}

	void Selection::ClearSelection() {
		GetService().Clear();
		ResolvedSelectionCache().clear();
		EmptySelection() = {};
	}

	void Selection::RemoveInvalidSelections() {
		if (const auto* selection = GetService().GetEntitySelection(); selection && selection->Entities.empty()) GetService().Clear();
	}

	void Selection::RemoveInvalidSelections(World& world) {
		if (!GetService().HasEntitySelection()) return;
		auto entities = GetSelectedEntityUuids();
		entities.erase(
			std::remove_if(entities.begin(), entities.end(), [&world](EntityUuid uuid) { return !world.GetEntity(uuid).IsValid(); }),
			entities.end());
		GetService().SelectEntities(std::move(entities));
	}

	void Selection::SelectAsset(AssetGuid guid) {
		GetService().SelectAsset(std::move(guid));
	}

	bool Selection::HasAssetSelection() {
		return GetService().HasAssetSelection();
	}

	AssetGuid Selection::GetSelectedAssetGuid() {
		const auto* selection = GetService().GetAssetSelection();
		return selection ? selection->Guid : AssetGuid{};
	}
}
