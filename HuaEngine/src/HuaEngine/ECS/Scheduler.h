#pragma once

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <vector>

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/ECS/System.h"

namespace HE {
	class Scheduler {
	public:
		void AddSystem(Ref<System> system) {
			if (!system) {
				return;
			}

			m_Systems.push_back({ system, system->Describe() });
			m_Order.clear();
		}

		bool Build() {
			m_Order.resize(m_Systems.size());
			std::iota(m_Order.begin(), m_Order.end(), size_t{0});

			std::stable_sort(m_Order.begin(), m_Order.end(), [this](size_t lhsIndex, size_t rhsIndex) {
				const auto& lhs = m_Systems[lhsIndex].Descriptor;
				const auto& rhs = m_Systems[rhsIndex].Descriptor;

				if (lhs.Stage != rhs.Stage) {
					return static_cast<int>(lhs.Stage) < static_cast<int>(rhs.Stage);
				}

				if (std::find(lhs.After.begin(), lhs.After.end(), rhs.Name) != lhs.After.end()) {
					return false;
				}

				if (std::find(rhs.After.begin(), rhs.After.end(), lhs.Name) != rhs.After.end()) {
					return true;
				}

				if (std::find(lhs.Before.begin(), lhs.Before.end(), rhs.Name) != lhs.Before.end()) {
					return true;
				}

				if (std::find(rhs.Before.begin(), rhs.Before.end(), lhs.Name) != rhs.Before.end()) {
					return false;
				}

				return lhs.Name < rhs.Name;
			});

			return true;
		}

		void Update(SystemContext& context) {
			if (m_Order.empty() && !m_Systems.empty()) {
				(void)Build();
			}

			for (const size_t systemIndex : m_Order) {
				auto& entry = m_Systems[systemIndex];
				if (entry.Descriptor.Enabled) {
					entry.Instance->Update(context);
				}
			}
		}

	private:
		struct Entry {
			Ref<System> Instance;
			SystemDescriptor Descriptor;
		};

		std::vector<Entry> m_Systems;
		std::vector<size_t> m_Order;
	};
}
