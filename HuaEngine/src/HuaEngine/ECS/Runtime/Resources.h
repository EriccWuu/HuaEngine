#pragma once

#include "HuaEngine/ECS/Runtime/TypeRegistry.h"

namespace HE::Ecs {
    using ResourceId = uint64_t;
    class ResourceRegistry;

    struct ResourceHandle {
        ResourceId Id = 0;
        const ResourceRegistry* Owner = nullptr;
        explicit operator bool() const noexcept { return Id != 0 && Owner != nullptr; }
        auto operator<=>(const ResourceHandle&) const = default;
    };

    struct RegisteredResource {
        ResourceId Id = 0;
        const void* NativeKey = nullptr;
        std::shared_ptr<void> Object;
    };

    class ResourceRegistry final {
    public:
        ResourceRegistry() = default;
        ResourceRegistry(const ResourceRegistry&) = delete;
        ResourceRegistry& operator=(const ResourceRegistry&) = delete;
        template<typename T>
        [[nodiscard]] Result<ResourceHandle> Register(std::shared_ptr<T> object) {
            static_assert(!std::is_const_v<T>);
            return RegisterObject(std::move(object), NativeTypeKey<T>());
        }
        [[nodiscard]] const RegisteredResource* Find(ResourceHandle handle) const;

    private:
        Result<ResourceHandle> RegisterObject(std::shared_ptr<void> object, const void* nativeKey);
        const std::thread::id m_OwnerThread = std::this_thread::get_id();
        mutable std::shared_mutex m_Mutex;
        std::vector<std::unique_ptr<RegisteredResource>> m_Entries;
        std::unordered_map<const void*, ResourceId> m_ByAddress;
    };
}
