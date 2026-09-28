#pragma once

#include "HuaEngine/ECS/Runtime/Result.h"

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace HE::Ecs::Detail {
    // Context-owned workers only take jobs whose scheduler dependencies are ready.
    class WorkerPool final {
    public:
        explicit WorkerPool(size_t count);
        ~WorkerPool();
        WorkerPool(const WorkerPool&) = delete;
        WorkerPool& operator=(const WorkerPool&) = delete;
        [[nodiscard]] size_t Size() const noexcept { return m_Workers.size(); }
        [[nodiscard]] Result<void> Enqueue(std::function<void()> job);
    private:
        void Run();
        std::mutex m_Mutex;
        std::condition_variable m_Wake;
        std::deque<std::function<void()>> m_Ready;
        std::vector<std::thread> m_Workers;
        bool m_Stopping = false;
    };
}
