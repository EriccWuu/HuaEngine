#include "enginepch.h"
#include "Interaction/EditorCommandRouter.h"

namespace HE {
    void EditorCommandRouter::Reset() {
        m_LastRoute.clear();
    }

    void EditorCommandRouter::SetLastRoute(std::string_view routeName) {
        m_LastRoute.assign(routeName);
    }
}
