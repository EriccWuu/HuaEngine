#pragma once

#include <string>
#include <string_view>

namespace HE {
    class EditorCommandRouter {
    public:
        void Reset();
        void SetLastRoute(std::string_view routeName);

        [[nodiscard]] const std::string& GetLastRoute() const { return m_LastRoute; }

    private:
        std::string m_LastRoute;
    };
}
