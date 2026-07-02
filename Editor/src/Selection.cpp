#include "enginepch.h"
#include "Selection.h"

#include <algorithm>

namespace HE {
	std::vector<EntityUuid> Selection::m_SelectedEntityUuids;
	std::vector<Entity> Selection::m_ResolvedSelections;
	Entity Selection::m_EmptySelection;

	void Selection::SetSelection(const Entity& selection) {
		SetSelectedEntity(selection.IsValid() ? selection.GetUuid() : EntityUuid{});
	}

	void Selection::SetSelectedEntity(EntityUuid uuid) {
		m_SelectedEntityUuids.clear();
		if (uuid != EntityUuid{}) {
			m_SelectedEntityUuids.push_back(uuid);
		}
	}

	void Selection::SetSelections(std::vector<Entity> selections) {
		m_SelectedEntityUuids.clear();
		m_SelectedEntityUuids.reserve(selections.size());
		for (const Entity& selection : selections) {
			if (selection.IsValid()) {
				m_SelectedEntityUuids.push_back(selection.GetUuid());
			}
		}
		RemoveInvalidSelections();
	}

	void Selection::SetSelectedEntities(std::vector<EntityUuid> selections) {
		m_SelectedEntityUuids = std::move(selections);
		RemoveInvalidSelections();
	}

	void Selection::AddToSelection(const Entity& selection) {
		if (!selection.IsValid() || IsSelected(selection)) {
			return;
		}

		m_SelectedEntityUuids.push_back(selection.GetUuid());
	}

	void Selection::ToggleSelection(const Entity& selection) {
		if (!selection.IsValid()) {
			return;
		}

		if (IsSelected(selection)) {
			RemoveFromSelection(selection);
			return;
		}

		m_SelectedEntityUuids.push_back(selection.GetUuid());
	}

	void Selection::RemoveFromSelection(const Entity& selection) {
		const EntityUuid uuid = selection.GetUuid();
		m_SelectedEntityUuids.erase(
			std::remove_if(
				m_SelectedEntityUuids.begin(),
				m_SelectedEntityUuids.end(),
				[uuid](EntityUuid candidate) {
					return candidate == uuid;
				}),
			m_SelectedEntityUuids.end());
	}

	Entity& Selection::GetSelection() {
		RemoveInvalidSelections();
		return m_EmptySelection;
	}

	const std::vector<Entity>& Selection::GetSelections() {
		RemoveInvalidSelections();
		m_ResolvedSelections.clear();
		return m_ResolvedSelections;
	}

	Entity Selection::ResolvePrimarySelection(World& world) {
		RemoveInvalidSelections(world);
		return m_SelectedEntityUuids.empty() ? Entity{} : world.GetEntity(m_SelectedEntityUuids.front());
	}

	const std::vector<Entity>& Selection::ResolveSelections(World& world) {
		RemoveInvalidSelections(world);
		m_ResolvedSelections.clear();
		m_ResolvedSelections.reserve(m_SelectedEntityUuids.size());
		for (EntityUuid uuid : m_SelectedEntityUuids) {
			Entity entity = world.GetEntity(uuid);
			if (entity.IsValid()) {
				m_ResolvedSelections.push_back(entity);
			}
		}
		return m_ResolvedSelections;
	}

	EntityUuid Selection::GetSelectedEntityUuid() {
		RemoveInvalidSelections();
		return m_SelectedEntityUuids.empty() ? EntityUuid{} : m_SelectedEntityUuids.front();
	}

	const std::vector<EntityUuid>& Selection::GetSelectedEntityUuids() {
		RemoveInvalidSelections();
		return m_SelectedEntityUuids;
	}

	bool Selection::HasSelection() {
		RemoveInvalidSelections();
		return !m_SelectedEntityUuids.empty();
	}

	bool Selection::HasSingleSelection() {
		RemoveInvalidSelections();
		return m_SelectedEntityUuids.size() == 1;
	}

	bool Selection::IsSelected(const Entity& selection) {
		RemoveInvalidSelections();
		if (!selection.IsValid()) {
			return false;
		}

		const EntityUuid uuid = selection.GetUuid();
		return std::any_of(
			m_SelectedEntityUuids.begin(),
			m_SelectedEntityUuids.end(),
			[uuid](EntityUuid candidate) {
				return candidate == uuid;
			});
	}

	size_t Selection::Count() {
		RemoveInvalidSelections();
		return m_SelectedEntityUuids.size();
	}

	void Selection::ClearSelection() {
		m_SelectedEntityUuids.clear();
		m_ResolvedSelections.clear();
		m_EmptySelection = {};
	}

	void Selection::RemoveInvalidSelections() {
		m_SelectedEntityUuids.erase(
			std::remove_if(
				m_SelectedEntityUuids.begin(),
				m_SelectedEntityUuids.end(),
				[](EntityUuid selection) {
					return selection == EntityUuid{};
				}),
			m_SelectedEntityUuids.end());
	}

	void Selection::RemoveInvalidSelections(World& world) {
		RemoveInvalidSelections();
		m_SelectedEntityUuids.erase(
			std::remove_if(
				m_SelectedEntityUuids.begin(),
				m_SelectedEntityUuids.end(),
				[&world](EntityUuid selection) {
					return !world.GetEntity(selection).IsValid();
				}),
			m_SelectedEntityUuids.end());
	}
}
