#include "enginepch.h"
#include "TestReflection.h"
#include <HuaEngine.h>

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/quaternion.hpp"

#include "HuaEngine/ECS/Components.h"

namespace HE {

    // For Test, remember to remove this
    class Person {
    public:
        void Pubfunc() {
            std::cout << "Pubfunc" << std::endl;
        }
        bool Pubfunc1(int, float, std::string) {
            std::cout << "Pubfunc1" << std::endl;
            return true;
        }

        int a = 0;
        const float b = 1.0;

    private:
        void Prifunc() {}
        bool Prifunc1(int, float, std::string) {}

        int m_a;
        float m_b;
    };

    void ReflectionTest::TestRefl() {
        TransformComponent p;
        auto typeInfo = Refl::reflect<TransformComponent>();

        typeInfo.visit_member_variables([&p](auto&& field) {
            std::cout << field.GetValue(&p) << std::endl;
        });

        typeInfo.visit_member_variables([&p](auto&& field) {
            field.SetValue(&p, glm::vec3(2));
        });

        typeInfo.visit_member_variables([&p](auto&& field) {
            std::cout << field.GetValue(&p) << std::endl;
        });
    }
}

srefl_class(Person,
    fields(
        field(a),
        field(b)
    )
)