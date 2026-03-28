#include "enginepch.h"
#include "Selection.h"

#include <algorithm>

namespace HE {
	std::vector<Entity> Selection::m_Selections;
	Entity Selection::m_EmptySelection;

	void Selection::SetSelection(const Entity& selection) {
		m_Selections.clear();
		if (selection.IsValid()) {
			m_Selections.push_back(selection);
		}
	}

	void Selection::SetSelections(std::vector<Entity> selections) {
		m_Selections = std::move(selections);
		RemoveInvalidSelections();
	}

	void Selection::AddToSelection(const Entity& selection) {
		if (!selection.IsValid() || IsSelected(selection)) {
			return;
		}

		m_Selections.push_back(selection);
	}

	void Selection::ToggleSelection(const Entity& selection) {
		if (!selection.IsValid()) {
			return;
		}

		if (IsSelected(selection)) {
			RemoveFromSelection(selection);
			return;
		}

		m_Selections.push_back(selection);
	}

	void Selection::RemoveFromSelection(const Entity& selection) {
		m_Selections.erase(
			std::remove_if(
				m_Selections.begin(),
				m_Selections.end(),
				[&selection](const Entity& candidate) {
					return candidate == selection;
				}),
			m_Selections.end());
	}

	Entity& Selection::GetSelection() {
		RemoveInvalidSelections();
		return m_Selections.empty() ? m_EmptySelection : m_Selections.front();
	}

	const std::vector<Entity>& Selection::GetSelections() {
		RemoveInvalidSelections();
		return m_Selections;
	}

	bool Selection::HasSelection() {
		RemoveInvalidSelections();
		return !m_Selections.empty();
	}

	bool Selection::HasSingleSelection() {
		RemoveInvalidSelections();
		return m_Selections.size() == 1;
	}

	bool Selection::IsSelected(const Entity& selection) {
		RemoveInvalidSelections();
		return std::any_of(
			m_Selections.begin(),
			m_Selections.end(),
			[&selection](const Entity& candidate) {
				return candidate == selection;
			});
	}

	size_t Selection::Count() {
		RemoveInvalidSelections();
		return m_Selections.size();
	}

	void Selection::ClearSelection() {
		m_Selections.clear();
		m_EmptySelection = {};
	}

	void Selection::RemoveInvalidSelections() {
		m_Selections.erase(
			std::remove_if(
				m_Selections.begin(),
				m_Selections.end(),
				[](const Entity& selection) {
					return !selection.IsValid();
				}),
			m_Selections.end());
	}
}
