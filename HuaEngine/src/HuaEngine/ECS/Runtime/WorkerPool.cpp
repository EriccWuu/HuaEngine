#include "HuaEngine/ECS/Runtime/WorkerPool.h"

#include <stdexcept>

namespace HE::Ecs::Detail {
    WorkerPool::WorkerPool(size_t count) {
        if (!count) throw std::invalid_argument("WorkerPool requires at least one worker");
        try {
            m_Workers.reserve(count);
            for (size_t index = 0; index < count; ++index) m_Workers.emplace_back([this] { Run(); });
        }
        catch (...) {
            { std::lock_guard lock(m_Mutex); m_Stopping = true; }
            m_Wake.notify_all();
            for (auto& worker : m_Workers) worker.join();
            throw;
        }
    }
    WorkerPool::~WorkerPool() {
        { std::lock_guard lock(m_Mutex); m_Stopping = true; }
        m_Wake.notify_all();
        for (auto& worker : m_Workers) worker.join();
    }
    Result<void> WorkerPool::Enqueue(std::function<void()> job) {
        if (!job) return Error{ErrorCode::InvalidArgument, "Enqueue", "A worker job is required"};
        try {
            { std::lock_guard lock(m_Mutex);
              if (m_Stopping) return Error{ErrorCode::InvalidState, "Enqueue", "The worker pool is stopping"};
              m_Ready.push_back(std::move(job)); }
            m_Wake.notify_one();
            return {};
        }
        catch (const std::exception& exception) { return Error{ErrorCode::ConstructionFailed, "Enqueue", exception.what()}; }
    }
    void WorkerPool::Run() {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock lock(m_Mutex);
                m_Wake.wait(lock, [this] { return m_Stopping || !m_Ready.empty(); });
                if (m_Ready.empty()) return;
                job = std::move(m_Ready.front());
                m_Ready.pop_front();
            }
            // Scheduler jobs report their own failures and wake dependents.
            job();
        }
    }
}
