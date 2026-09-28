#pragma once

#include "HuaEngine/ECS/Runtime/Query.h"
#include "HuaEngine/ECS/Runtime/Commands.h"

#include <atomic>
#include <optional>
#include <type_traits>
#include <utility>

namespace HE::Ecs {
    enum class TimelineMode { Serial, Parallel };
    struct TimelineOptions {
        TimelineMode Mode = TimelineMode::Parallel;
        size_t RowsPerBatch = 256;
        size_t SmallTaskRows = 512;
    };
    enum class TaskStatus { Invalid, Queued, Running, Succeeded, Failed, Cancelled };
    namespace Detail { struct TaskCompletion; }

    class TaskHandle final {
    public:
        TaskHandle() = default;
        [[nodiscard]] explicit operator bool() const noexcept;
        [[nodiscard]] uint64_t Sequence() const noexcept;
        [[nodiscard]] TaskStatus Status() const noexcept;
        [[nodiscard]] bool IsComplete() const noexcept;
    private:
        friend class Timeline;
        explicit TaskHandle(std::shared_ptr<Detail::TaskCompletion> completion) : m_Completion(std::move(completion)) {}
        std::shared_ptr<Detail::TaskCompletion> m_Completion;
    };

    class TaskBatch final {
    public:
        [[nodiscard]] QueryBatch& View() noexcept { return m_View; }
        [[nodiscard]] CommandBuffer& Commands() noexcept { return m_Commands; }
        [[nodiscard]] uint64_t Sequence() const noexcept { return m_Sequence; }
        [[nodiscard]] size_t BatchIndex() const noexcept { return m_BatchIndex; }
    private:
        friend class Timeline;
        TaskBatch(QueryBatch& view, CommandBuffer& commands, uint64_t sequence, size_t batchIndex)
            : m_View(view), m_Commands(commands), m_Sequence(sequence), m_BatchIndex(batchIndex) {}
        QueryBatch& m_View;
        CommandBuffer& m_Commands;
        uint64_t m_Sequence;
        size_t m_BatchIndex;
    };

    class Timeline final {
    public:
        // Invalid Context ownership, a second active Timeline, or unsupported options throw.
        explicit Timeline(EcsContext& context, TimelineOptions options = {});
        ~Timeline();
        Timeline(const Timeline&) = delete;
        Timeline& operator=(const Timeline&) = delete;
        Timeline(Timeline&&) = delete;
        Timeline& operator=(Timeline&&) = delete;

        template<typename Callback>
        [[nodiscard]] Result<TaskHandle> Submit(Query& query, World& world, Callback&& callback, ChangedState* changed = nullptr) {
            using Owned = std::decay_t<Callback>;
            struct Model final : CallbackBase {
                explicit Model(Owned value) : Value(std::move(value)) {}
                Result<void> Invoke(TaskBatch& batch) override {
                    if constexpr (std::is_invocable_r_v<Result<void>, const Owned&, TaskBatch&>) return std::invoke(std::as_const(Value), batch);
                    else return std::invoke(Value, batch);
                }
                bool ConcurrentSafe() const noexcept override { return std::is_invocable_r_v<Result<void>, const Owned&, TaskBatch&>; }
                Owned Value;
            };
            try { return SubmitOwned(query, world, std::make_unique<Model>(std::forward<Callback>(callback)), changed); }
            catch (const std::exception& exception) { return Error{ErrorCode::ConstructionFailed, "Submit", exception.what()}; }
            catch (...) { return Error{ErrorCode::ConstructionFailed, "Submit", "Callback ownership construction failed"}; }
        }
        [[nodiscard]] Result<void> Wait(const TaskHandle& task);
        [[nodiscard]] Result<void> Finish();
        [[nodiscard]] Result<void> CommitCommands();
        [[nodiscard]] Result<void> ResetError();
        [[nodiscard]] Result<EntityId> Resolve(const TaskHandle& task, TemporaryEntity entity) const;
        [[nodiscard]] std::optional<Error> LastError() const;
        [[nodiscard]] bool IsIdle() const noexcept;
        [[nodiscard]] Result<void> Synchronize(World& world);

    private:
        friend class World;
        friend class WorldAccessScope;
        struct CallbackBase {
            virtual ~CallbackBase() = default;
            virtual Result<void> Invoke(TaskBatch&) = 0;
            virtual bool ConcurrentSafe() const noexcept = 0;
        };
        struct Impl;
        [[nodiscard]] Result<TaskHandle> SubmitOwned(Query&, World&, std::unique_ptr<CallbackBase>, ChangedState*);
        void OnWorldDestroy(World& world) noexcept;
        std::shared_ptr<Impl> m_Impl;
    };
}
