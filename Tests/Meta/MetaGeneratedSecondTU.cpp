#include "Fixtures/ComponentModule.h"
#include <Test/GeneratedEcs.h>

#include "HuaEngine/ECS/Runtime/EcsContext.h"

HE::Ecs::Result<HE::Ecs::TypeId> RegisterPlainFromSecondTranslationUnit(HE::Ecs::EcsContext& context) {
    return context.Types().Register<P6Fixture::PlainComponent>();
}
