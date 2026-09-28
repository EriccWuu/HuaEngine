#include "HuaEngine/ECS/Runtime/Timeline.h"
#include "HuaEngine/ECS/Runtime/QuerySubmission.h"
#include "HuaEngine/ECS/Runtime/WorkerPool.h"

#include <algorithm>
#include <condition_variable>
#include <map>
#include <mutex>
#include <stdexcept>
#include <tuple>

namespace HE::Ecs {
    namespace {
        std::atomic<uint64_t> NextTimeline{1};
        uint64_t AllocateTimeline() noexcept {
            auto current = NextTimeline.load();
            while (current != std::numeric_limits<uint64_t>::max()) {
                if (NextTimeline.compare_exchange_weak(current, current + 1)) return current;
            }
            return 0;
        }
        bool Complete(TaskStatus status) noexcept {
            return status == TaskStatus::Succeeded || status == TaskStatus::Failed || status == TaskStatus::Cancelled;
        }
        bool Conflicting(AccessMode a, AccessMode b) noexcept { return a == AccessMode::Write || b == AccessMode::Write; }
        struct ExecutionGuard {
            std::atomic<bool>& Flag;
            bool Previous;
            explicit ExecutionGuard(std::atomic<bool>& flag) noexcept : Flag(flag), Previous(flag.exchange(true)) {}
            ~ExecutionGuard() { Flag.store(Previous); }
        };
        Error Problem(ErrorCode code, std::string operation, std::string message) { return {code, std::move(operation), std::move(message)}; }
    }
    namespace Detail {
        struct TaskCompletion {
            uint64_t Timeline = 0;
            uint64_t Sequence = 0;
            std::atomic<TaskStatus> Status{TaskStatus::Queued};
            std::optional<Error> Failure;
            std::vector<std::shared_ptr<CommandBuffer>> Mappings;
        };
    }
    TaskHandle::operator bool() const noexcept { return static_cast<bool>(m_Completion); }
    uint64_t TaskHandle::Sequence() const noexcept { return m_Completion ? m_Completion->Sequence : 0; }
    TaskStatus TaskHandle::Status() const noexcept { return m_Completion ? m_Completion->Status.load(std::memory_order_acquire) : TaskStatus::Invalid; }
    bool TaskHandle::IsComplete() const noexcept { return Complete(Status()); }

    struct Timeline::Impl : Detail::TimelineControl, std::enable_shared_from_this<Impl> {
        struct BatchCommands {
            ChunkHandle Chunk;
            size_t Index;
            std::shared_ptr<CommandBuffer> Buffer;
        };
        struct Task;
        struct Node;
        struct Work {
            enum class Kind { Start, Batch } Type = Kind::Start;
            Task* Owner = nullptr;
            Node* Signal = nullptr;
            size_t Batch = 0;
            Work* Next = nullptr;
        };
        struct Node {
            Task* Owner = nullptr;
            std::vector<size_t> Chunks;
            std::vector<Detail::LocalAccess> Local;
            std::vector<std::weak_ptr<Node>> Successors;
            Work Start;
            size_t Pending = 0;
            size_t Children = 0;
            bool Blocked = false;
            bool Completed = false;
            std::optional<Error> FirstError;
        };
        struct Frontier {
            std::weak_ptr<Node> Writer;
            std::vector<std::weak_ptr<Node>> Readers;
        };
        using LocalKey = std::tuple<WorldId, TypeId, ChunkId, uint64_t>;
        using DomainKey = std::pair<WorldId, TypeId>;
        struct Task : std::enable_shared_from_this<Task> {
            std::shared_ptr<Detail::TaskCompletion> Completion;
            Detail::QuerySubmission Submission;
            std::optional<Detail::PreparedExecution> Prepared;
            std::unique_ptr<CallbackBase> Callback;
            WorldId MainId = 0;
            std::weak_ptr<const Detail::WorldLifetime> MainLifetime;
            std::vector<WorldId> Worlds;
            std::vector<std::shared_ptr<Detail::TaskCompletion>> Dependencies;
            std::vector<Detail::SubmittedRandomAccess> Random;
            std::vector<ResourceAccess> Resources;
            uint64_t Consumer = 0;
            std::vector<BatchCommands> Commands;
            std::vector<std::shared_ptr<Node>> Nodes;
            std::shared_ptr<Node> Aggregate;
            std::vector<Work> BatchJobs;
            size_t RemainingNodes = 0;
            std::optional<Error> FirstFailure;
        };
        EcsContext& Context;
        const TimelineOptions Options;
        uint64_t Identity = AllocateTimeline();
        uint64_t NextSequence = 1;
        std::atomic<bool> Executing{false};
        std::atomic<bool> Closing{false};
        std::atomic<bool> ClosingFinalizing{false};
        std::atomic<bool> Closed{false};
        std::optional<Error> Failure;
        std::vector<std::shared_ptr<Task>> Tasks;
        std::mutex GraphMutex;
        std::condition_variable GraphWake;
        std::condition_variable ReadyWake;
        std::map<LocalKey, Frontier> LocalFrontiers;
        std::map<DomainKey, Frontier> RandomFrontiers;
        std::map<ResourceId, Frontier> ResourceFrontiers;
        std::map<uint64_t, std::weak_ptr<Node>> ConsumerFrontiers;
        Work* ReadyHead = nullptr;
        Work* ReadyTail = nullptr;
        size_t Runners = 0;
        bool StopWorkers = false;

        explicit Impl(EcsContext& context, TimelineOptions options) : Context(context), Options(options) {}
        [[nodiscard]] bool Parallel() const noexcept { return Options.Mode == TimelineMode::Parallel; }
        Result<void> Check(std::string operation) const {
            if (!Context.IsMainThread()) return Problem(ErrorCode::WrongThread, std::move(operation), "Timeline operations require the Context main thread");
            if (Executing.load()) return Problem(ErrorCode::Busy, std::move(operation), "A task cannot reenter its Timeline");
            return {};
        }
        [[nodiscard]] Result<void> StartRunners();
        void StopRunners() noexcept;
        void Runner() noexcept;
        void QueueLocked(Work& work) noexcept;
        void CompleteSignalLocked(Node& node, bool failed) noexcept;
        [[nodiscard]] Result<void> SubmitParallel(std::shared_ptr<Task> task);
        [[nodiscard]] Result<void> InvokeBatch(Task& task, size_t index);
        void RunStart(Node& node) noexcept;
        void RunBatchWork(Node& node, size_t index) noexcept;
        void FinishBatch(Node& node, std::optional<Error> error) noexcept;
        void CompleteNode(Node& node, std::optional<Error> error) noexcept;
        void FinalizeTask(Task& task) noexcept;
        [[nodiscard]] Result<void> WaitParallel(const std::shared_ptr<Detail::TaskCompletion>& completion);
        [[nodiscard]] Result<void> DrainParallel();
        void RetireParallel();
        [[nodiscard]] std::optional<Error> FailureSnapshot();
        static bool Depends(const Detail::QuerySubmission& later, const Detail::QuerySubmission& earlier) {
            if (later.ConsumerId() && later.ConsumerId() == earlier.ConsumerId()) return true;
            if (later.MainId() == earlier.MainId()) {
                for (const auto& a : later.LocalAccesses()) for (const auto& b : earlier.LocalAccesses()) {
                    if (a.Chunk == b.Chunk && a.Type == b.Type && Conflicting(a.Access, b.Access)) return true;
                }
            }
            for (const auto& a : later.RandomAccesses()) {
                for (const auto& b : earlier.RandomAccesses()) {
                    if (a.World == b.World && a.Type == b.Type && Conflicting(a.Access, b.Access)) return true;
                }
                if (a.World == earlier.MainId()) for (const auto& b : earlier.LocalAccesses()) {
                    if (a.Type == b.Type && Conflicting(a.Access, b.Access)) return true;
                }
            }
            for (const auto& b : earlier.RandomAccesses()) {
                if (b.World == later.MainId()) for (const auto& a : later.LocalAccesses()) {
                    if (a.Type == b.Type && Conflicting(a.Access, b.Access)) return true;
                }
            }
            for (const auto& a : later.ResourceAccesses()) for (const auto& b : earlier.ResourceAccesses()) {
                if (a.Resource == b.Resource && Conflicting(a.Access, b.Access)) return true;
            }
            return false;
        }
        void DiscardCommands() noexcept {
            ExecutionGuard guard(Executing);
            for (auto& task : Tasks) {
                std::erase_if(task->Completion->Mappings, [](const auto& buffer) { return !buffer->Consumed(); });
                task->Commands.clear();
            }
        }
        void Retire() {
            std::erase_if(Tasks, [](const auto& task) {
                if (!Complete(task->Completion->Status.load())) return false;
                return std::all_of(task->Commands.begin(), task->Commands.end(), [](const auto& batch) {
                    return batch.Buffer->Consumed() || batch.Buffer->CommandCount() == 0;
                });
            });
        }
        void Execute(Task& task) {
            if (Complete(task.Completion->Status.load())) return;
            ExecutionGuard guard(Executing);
            for (const auto& dependency : task.Dependencies) {
                const auto status = dependency->Status.load();
                if (status == TaskStatus::Failed || status == TaskStatus::Cancelled) {
                    Error error = Problem(ErrorCode::Cancelled, "ExecuteTask", "A prerequisite task failed: " + std::to_string(dependency->Sequence));
                    error.Task = task.Completion->Sequence;
                    task.Completion->Failure = std::move(error);
                    for (size_t chunk = 0; chunk < task.Prepared->Chunks().size(); ++chunk) (void)task.Prepared->CompleteChunk(chunk, true);
                    (void)task.Prepared->Finish(false);
                    task.Prepared.reset();
                    task.Submission.Release();
                    task.Callback.reset();
                    task.Dependencies.clear();
                    task.Completion->Status.store(TaskStatus::Cancelled, std::memory_order_release);
                    Context.m_ScheduledWork.fetch_sub(1, std::memory_order_release);
                    return;
                }
            }
            task.Completion->Status.store(TaskStatus::Running, std::memory_order_release);
            Result<void> result;
            try {
                const auto chunks = task.Prepared->Chunks();
                for (size_t chunk = 0; chunk < chunks.size(); ++chunk) {
                    if (!result) {
                        (void)task.Prepared->CompleteChunk(chunk, true);
                        continue;
                    }
                    auto begin = task.Prepared->BeginChunk(chunk);
                    if (!begin) {
                        result = begin.GetError();
                        (void)task.Prepared->CompleteChunk(chunk, true);
                        continue;
                    }
                    bool cancelled = false;
                    if (begin.Value()) {
                        for (size_t batch = chunks[chunk].FirstBatch;
                            batch < chunks[chunk].FirstBatch + chunks[chunk].BatchCount; ++batch) {
                            auto run = task.Prepared->RunBatch(batch, [&](QueryBatch& view) -> Result<void> {
                                const size_t index = task.Prepared->Batches()[batch].Index;
                                auto buffer = std::make_shared<CommandBuffer>(Context);
                                task.Commands.push_back({view.Chunk(), index, buffer});
                                task.Completion->Mappings.push_back(buffer);
                                TaskBatch current(view, *buffer, task.Completion->Sequence, index);
                                return task.Callback->Invoke(current);
                            });
                            if (!run) { result = run.GetError(); cancelled = true; break; }
                        }
                    }
                    auto completed = task.Prepared->CompleteChunk(chunk, cancelled);
                    if (!completed && result) result = completed.GetError();
                }
                auto finished = task.Prepared->Finish(static_cast<bool>(result));
                if (!finished && result) result = finished.GetError();
            }
            catch (const std::exception& exception) { result = Problem(ErrorCode::TaskFailed, "ExecuteTask", exception.what()); }
            catch (...) { result = Problem(ErrorCode::TaskFailed, "ExecuteTask", "The task threw a non-standard exception"); }
            task.Prepared.reset();
            task.Submission.Release();
            task.Callback.reset();
            task.Dependencies.clear();
            if (!result) {
                auto error = std::move(result.GetError());
                error.Task = task.Completion->Sequence;
                if (!Failure) Failure = error;
                task.Completion->Failure = std::move(error);
                task.Completion->Status.store(TaskStatus::Failed, std::memory_order_release);
            }
            else task.Completion->Status.store(TaskStatus::Succeeded, std::memory_order_release);
            Context.m_ScheduledWork.fetch_sub(1, std::memory_order_release);
        }
        void RunThrough(uint64_t sequence) {
            for (auto& task : Tasks) {
                if (task->Completion->Sequence > sequence && !Closing) break;
                Execute(*task);
            }
            FinishClosing();
        }
        void FinishClosing() noexcept {
            if (!Closing.load() || Context.m_ScheduledWork.load(std::memory_order_acquire)) return;
            if (Parallel()) {
                bool expected = false;
                if (!ClosingFinalizing.compare_exchange_strong(expected, true)) return;
                std::vector<std::shared_ptr<Task>> detached;
                {
                    std::lock_guard graph(GraphMutex);
                    if (!Closing.load() || Context.m_ScheduledWork.load(std::memory_order_acquire)) {
                        ClosingFinalizing.store(false);
                        return;
                    }
                    detached.swap(Tasks);
                    StopWorkers = true;
                    Closed.store(true);
                    ReadyWake.notify_all();
                }
                for (auto& task : detached) {
                    std::erase_if(task->Completion->Mappings, [](const auto& buffer) { return !buffer->Consumed(); });
                    task->Commands.clear();
                }
                detached.clear();
            }
            else {
                DiscardCommands();
                Tasks.clear();
                Closed.store(true);
            }
            {
                std::lock_guard lock(Context.m_TimelineMutex);
                if (Context.m_TimelineControl.get() == this) Context.m_TimelineControl.reset();
            }
            if (Parallel()) {
                {
                    std::lock_guard graph(GraphMutex);
                    Closing.store(false);
                    Context.m_ClosingTimeline.store(false, std::memory_order_release);
                }
                GraphWake.notify_all();
            }
            else {
                Closing.store(false);
                Context.m_ClosingTimeline.store(false, std::memory_order_release);
            }
        }
        Result<void> SynchronizeWorld(World& world) override {
            if (auto ready = Check("Synchronize"); !ready) return ready;
            if (&world.Context() != &Context) return Problem(ErrorCode::InvalidArgument, "Synchronize", "The World belongs to another Context");
            if (Parallel()) {
                const WorldId id = world.Id();
                std::unique_lock lock(GraphMutex);
                GraphWake.wait(lock, [&] {
                    return std::all_of(Tasks.begin(), Tasks.end(), [&](const auto& task) {
                        return !std::binary_search(task->Worlds.begin(), task->Worlds.end(), id) ||
                            Complete(task->Completion->Status.load(std::memory_order_acquire));
                    }) && !Context.m_ClosingTimeline.load(std::memory_order_acquire);
                });
                auto failure = Failure;
                lock.unlock();
                RetireParallel();
                FinishClosing();
                if (failure) return *failure;
                return {};
            }
            uint64_t through = 0;
            const WorldId id = world.Id();
            for (const auto& task : Tasks) {
                if (std::binary_search(task->Worlds.begin(), task->Worlds.end(), id)) through = task->Completion->Sequence;
            }
            RunThrough(through);
            Retire();
            FinishClosing();
            if (Failure) return *Failure;
            return {};
        }
        void WorldDestroyed(World& world) noexcept override {
            if (Executing.load() || !Context.IsMainThread()) return;
            try { (void)SynchronizeWorld(world); } catch (...) {}
        }
    };

    Result<void> Timeline::Impl::StartRunners() {
        auto self = shared_from_this();
        try {
            auto& pool = Context.Workers();
            for (size_t index = 0; index < Context.WorkerCount(); ++index) {
                std::function<void()> job = [self] { self->Runner(); };
                {
                    std::lock_guard lock(GraphMutex);
                    ++Runners;
                }
                auto queued = pool.Enqueue(std::move(job));
                if (!queued) {
                    {
                        std::lock_guard lock(GraphMutex);
                        --Runners;
                    }
                    StopRunners();
                    return queued;
                }
            }
            return {};
        }
        catch (const std::exception& exception) {
            StopRunners();
            return Problem(ErrorCode::ConstructionFailed, "Timeline", exception.what());
        }
    }
    void Timeline::Impl::StopRunners() noexcept {
        std::unique_lock lock(GraphMutex);
        StopWorkers = true;
        ReadyWake.notify_all();
        GraphWake.wait(lock, [&] { return Runners == 0; });
    }
    void Timeline::Impl::QueueLocked(Work& work) noexcept {
        work.Next = nullptr;
        if (ReadyTail) ReadyTail->Next = &work;
        else ReadyHead = &work;
        ReadyTail = &work;
        ReadyWake.notify_one();
    }
    void Timeline::Impl::CompleteSignalLocked(Node& node, bool failed) noexcept {
        node.Completed = true;
        for (const auto& weak : node.Successors) {
            if (const auto next = weak.lock()) {
                if (failed) next->Blocked = true;
                if (--next->Pending == 0) QueueLocked(next->Start);
            }
        }
    }
    void Timeline::Impl::Runner() noexcept {
        for (;;) {
            Work* work = nullptr;
            std::shared_ptr<Task> task;
            {
                std::unique_lock lock(GraphMutex);
                ReadyWake.wait(lock, [&] { return StopWorkers || ReadyHead; });
                if (!ReadyHead && StopWorkers) break;
                work = ReadyHead;
                ReadyHead = work->Next;
                if (!ReadyHead) ReadyTail = nullptr;
                work->Next = nullptr;
                task = work->Owner->shared_from_this();
            }
            try {
                if (work->Type == Work::Kind::Start) RunStart(*work->Signal);
                else RunBatchWork(*work->Signal, work->Batch);
            }
            catch (const std::exception& exception) {
                Error error = Problem(ErrorCode::TaskFailed, "ExecuteTask", exception.what());
                error.Task = task->Completion->Sequence;
                if (work->Type == Work::Kind::Batch) FinishBatch(*work->Signal, std::move(error));
                else CompleteNode(*work->Signal, std::move(error));
            }
            catch (...) {
                Error error = Problem(ErrorCode::TaskFailed, "ExecuteTask", "The worker failed with a non-standard exception");
                error.Task = task->Completion->Sequence;
                if (work->Type == Work::Kind::Batch) FinishBatch(*work->Signal, std::move(error));
                else CompleteNode(*work->Signal, std::move(error));
            }
        }
        {
            std::lock_guard lock(GraphMutex);
            --Runners;
        }
        GraphWake.notify_all();
    }
    Result<void> Timeline::Impl::SubmitParallel(std::shared_ptr<Task> task) {
        if (!task->Prepared) return Problem(ErrorCode::InvalidState, "Submit", "The task has no prepared execution");
        const auto chunks = task->Prepared->Chunks();
        const auto batches = task->Prepared->Batches();
        const bool merged = task->Prepared->MergeSmallTask() || task->Prepared->RequiresSerial() || !task->Callback->ConcurrentSafe();
        std::vector<Node*> owners(chunks.size());
        const auto makeNode = [&]() {
            auto node = std::make_shared<Node>();
            node->Owner = task.get();
            node->Start = {Work::Kind::Start, task.get(), node.get(), 0, nullptr};
            task->Nodes.push_back(node);
            return node;
        };
        if (merged || chunks.empty()) {
            const auto node = makeNode();
            for (size_t index = 0; index < chunks.size(); ++index) {
                owners[index] = node.get();
                node->Chunks.push_back(index);
                node->Local.insert(node->Local.end(), chunks[index].Accesses.begin(), chunks[index].Accesses.end());
            }
        }
        else {
            // Keep dependency keys per chunk while avoiding one worker job per tiny chunk.
            std::shared_ptr<Node> node;
            size_t rows = 0;
            for (size_t index = 0; index < chunks.size(); ++index) {
                const bool nextUnit = Options.SmallTaskRows == 0 ||
                    (rows != 0 && (rows >= Options.SmallTaskRows ||
                        chunks[index].Rows > Options.SmallTaskRows - rows));
                if (!node || nextUnit) {
                    node = makeNode();
                    rows = 0;
                }
                owners[index] = node.get();
                node->Chunks.push_back(index);
                node->Local.insert(node->Local.end(), chunks[index].Accesses.begin(), chunks[index].Accesses.end());
                rows += chunks[index].Rows;
            }
        }
        task->Aggregate = std::make_shared<Node>();
        task->Aggregate->Owner = task.get();
        task->RemainingNodes = task->Nodes.size();
        task->BatchJobs.resize(batches.size());
        task->Commands.reserve(batches.size());
        task->Completion->Mappings.reserve(batches.size());
        for (size_t index = 0; index < batches.size(); ++index) {
            const auto& batch = batches[index];
            auto buffer = std::make_shared<CommandBuffer>(Context);
            task->Commands.push_back({chunks[batch.ChunkIndex].Chunk, batch.Index, buffer});
            task->Completion->Mappings.push_back(buffer);
            task->BatchJobs[index] = {Work::Kind::Batch, task.get(), owners[batch.ChunkIndex], index, nullptr};
        }
        struct Edge { std::shared_ptr<Node> Before; std::shared_ptr<Node> After; };
        std::unique_lock lock(GraphMutex);
        if (Failure) return *Failure;
        std::vector<Edge> edges;
        const auto add = [&](const std::weak_ptr<Node>& before, const std::shared_ptr<Node>& after) {
            const auto prior = before.lock();
            if (prior && !prior->Completed && prior.get() != after.get()) edges.push_back({prior, after});
        };
        const auto collect = [&](const Frontier& frontier, AccessMode mode, const std::shared_ptr<Node>& after) {
            add(frontier.Writer, after);
            if (mode == AccessMode::Write) for (const auto& reader : frontier.Readers) add(reader, after);
        };
        for (const auto& node : task->Nodes) {
            for (const auto& local : node->Local) {
                const LocalKey key{task->MainId, local.Type, local.Chunk.Id, local.Chunk.Generation};
                if (const auto found = LocalFrontiers.find(key); found != LocalFrontiers.end()) collect(found->second, local.Access, node);
                if (const auto found = RandomFrontiers.find({task->MainId, local.Type}); found != RandomFrontiers.end()) {
                    collect(found->second, local.Access, node);
                }
            }
            for (const auto& random : task->Random) {
                if (const auto found = RandomFrontiers.find({random.World, random.Type}); found != RandomFrontiers.end()) {
                    collect(found->second, random.Access, node);
                }
                auto found = LocalFrontiers.lower_bound(LocalKey{random.World, random.Type, 0, 0});
                for (; found != LocalFrontiers.end() && std::get<0>(found->first) == random.World &&
                    std::get<1>(found->first) == random.Type; ++found) {
                    collect(found->second, random.Access, node);
                }
            }
            for (const auto& resource : task->Resources) {
                if (const auto found = ResourceFrontiers.find(resource.Resource.Id); found != ResourceFrontiers.end()) {
                    collect(found->second, resource.Access, node);
                }
            }
            if (task->Consumer) {
                if (const auto found = ConsumerFrontiers.find(task->Consumer); found != ConsumerFrontiers.end()) add(found->second, node);
            }
        }
        std::sort(edges.begin(), edges.end(), [](const Edge& left, const Edge& right) {
            if (left.Before.get() != right.Before.get()) return std::less<Node*>{}(left.Before.get(), right.Before.get());
            return std::less<Node*>{}(left.After.get(), right.After.get());
        });
        edges.erase(std::unique(edges.begin(), edges.end(), [](const Edge& left, const Edge& right) {
            return left.Before.get() == right.Before.get() && left.After.get() == right.After.get();
        }), edges.end());

        std::map<LocalKey, Frontier> localUpdates;
        std::map<DomainKey, Frontier> randomUpdates;
        std::map<ResourceId, Frontier> resourceUpdates;
        const auto staged = [](auto& changes, auto& current, const auto& key) -> Frontier& {
            auto found = changes.find(key);
            if (found == changes.end()) {
                const auto old = current.find(key);
                found = changes.emplace(key, old == current.end() ? Frontier{} : old->second).first;
                auto& value = found->second;
                if (const auto writer = value.Writer.lock(); !writer || writer->Completed) value.Writer.reset();
                std::erase_if(value.Readers, [](const auto& weak) {
                    const auto reader = weak.lock();
                    return !reader || reader->Completed;
                });
            }
            return found->second;
        };
        const auto append = [](Frontier& frontier, AccessMode access, const std::shared_ptr<Node>& signal) {
            if (access == AccessMode::Write) {
                frontier.Writer = signal;
                frontier.Readers.clear();
            }
            else frontier.Readers.push_back(signal);
        };
        for (const auto& node : task->Nodes) for (const auto& local : node->Local) {
            const LocalKey key{task->MainId, local.Type, local.Chunk.Id, local.Chunk.Generation};
            append(staged(localUpdates, LocalFrontiers, key), local.Access, node);
        }
        for (const auto& random : task->Random) {
            append(staged(randomUpdates, RandomFrontiers, DomainKey{random.World, random.Type}), random.Access, task->Aggregate);
        }
        for (const auto& resource : task->Resources) {
            append(staged(resourceUpdates, ResourceFrontiers, resource.Resource.Id), resource.Access, task->Aggregate);
        }
        for (const auto& [key, value] : localUpdates) LocalFrontiers.try_emplace(key);
        for (const auto& [key, value] : randomUpdates) RandomFrontiers.try_emplace(key);
        for (const auto& [key, value] : resourceUpdates) ResourceFrontiers.try_emplace(key);
        if (task->Consumer) ConsumerFrontiers.try_emplace(task->Consumer);
        std::map<Node*, size_t> edgeCounts;
        for (const auto& edge : edges) ++edgeCounts[edge.Before.get()];
        Tasks.reserve(Tasks.size() + 1);
        for (const auto& [node, count] : edgeCounts) node->Successors.reserve(node->Successors.size() + count);
        for (const auto& edge : edges) {
            edge.Before->Successors.push_back(edge.After);
            ++edge.After->Pending;
        }
        for (auto& [key, value] : localUpdates) std::swap(LocalFrontiers.at(key), value);
        for (auto& [key, value] : randomUpdates) std::swap(RandomFrontiers.at(key), value);
        for (auto& [key, value] : resourceUpdates) std::swap(ResourceFrontiers.at(key), value);
        if (task->Consumer) ConsumerFrontiers.at(task->Consumer) = task->Aggregate;
        Tasks.push_back(std::move(task));
        ++NextSequence;
        Context.m_ScheduledWork.fetch_add(1, std::memory_order_release);
        for (const auto& node : Tasks.back()->Nodes) if (!node->Pending) QueueLocked(node->Start);
        return {};
    }
    Result<void> Timeline::Impl::InvokeBatch(Task& task, size_t index) {
        try {
            return task.Prepared->RunBatch(index, [&](QueryBatch& view) -> Result<void> {
                auto& command = task.Commands[index];
                TaskBatch batch(view, *command.Buffer, task.Completion->Sequence, command.Index);
                return task.Callback->Invoke(batch);
            });
        }
        catch (const std::exception& exception) { return Problem(ErrorCode::TaskFailed, "ExecuteTask", exception.what()); }
        catch (...) { return Problem(ErrorCode::TaskFailed, "ExecuteTask", "The batch threw a non-standard exception"); }
    }
    void Timeline::Impl::RunStart(Node& node) noexcept {
        auto task = node.Owner->shared_from_this();
        task->Completion->Status.store(TaskStatus::Running, std::memory_order_release);
        std::optional<Error> error;
        try {
            if (node.Blocked) {
                for (const size_t chunk : node.Chunks) (void)task->Prepared->CompleteChunk(chunk, true);
                error = Problem(ErrorCode::Cancelled, "ExecuteTask", "A prerequisite task failed");
            }
            else if (node.Chunks.size() != 1 || task->Prepared->MergeSmallTask() || task->Prepared->RequiresSerial() || !task->Callback->ConcurrentSafe()) {
                const auto plans = task->Prepared->Chunks();
                for (size_t at = 0; at < node.Chunks.size(); ++at) {
                    const size_t chunk = node.Chunks[at];
                    if (error) {
                        (void)task->Prepared->CompleteChunk(chunk, true);
                        continue;
                    }
                    auto begin = task->Prepared->BeginChunk(chunk);
                    if (!begin) {
                        error = begin.GetError();
                        (void)task->Prepared->CompleteChunk(chunk, true);
                        continue;
                    }
                    bool cancelled = false;
                    if (begin.Value()) {
                        const auto& plan = plans[chunk];
                        for (size_t batch = plan.FirstBatch; batch < plan.FirstBatch + plan.BatchCount; ++batch) {
                            auto run = InvokeBatch(*task, batch);
                            if (!run) { error = run.GetError(); cancelled = true; break; }
                        }
                    }
                    auto completed = task->Prepared->CompleteChunk(chunk, cancelled);
                    if (!completed && !error) error = completed.GetError();
                }
            }
            else {
                const size_t chunk = node.Chunks.front();
                auto begin = task->Prepared->BeginChunk(chunk);
                if (!begin) {
                    error = begin.GetError();
                    (void)task->Prepared->CompleteChunk(chunk, true);
                }
                else if (!begin.Value()) {
                    auto completed = task->Prepared->CompleteChunk(chunk);
                    if (!completed) error = completed.GetError();
                }
                else {
                    const auto& plan = task->Prepared->Chunks()[chunk];
                    if (plan.BatchCount == 1) {
                        auto run = InvokeBatch(*task, plan.FirstBatch);
                        if (!run) error = run.GetError();
                        auto completed = task->Prepared->CompleteChunk(chunk);
                        if (!completed && !error) error = completed.GetError();
                    }
                    else if (plan.BatchCount) {
                        std::lock_guard lock(GraphMutex);
                        node.Children = plan.BatchCount;
                        for (size_t batch = plan.FirstBatch; batch < plan.FirstBatch + plan.BatchCount; ++batch) {
                            QueueLocked(task->BatchJobs[batch]);
                        }
                        return;
                    }
                    else {
                        auto completed = task->Prepared->CompleteChunk(chunk);
                        if (!completed) error = completed.GetError();
                    }
                }
            }
        }
        catch (const std::exception& exception) { error = Problem(ErrorCode::TaskFailed, "ExecuteTask", exception.what()); }
        catch (...) { error = Problem(ErrorCode::TaskFailed, "ExecuteTask", "The Chunk failed with a non-standard exception"); }
        CompleteNode(node, std::move(error));
    }
    void Timeline::Impl::RunBatchWork(Node& node, size_t index) noexcept {
        std::optional<Error> error;
        try {
            auto result = InvokeBatch(*node.Owner, index);
            if (!result) error = result.GetError();
        }
        catch (const std::exception& exception) { error = Problem(ErrorCode::TaskFailed, "ExecuteTask", exception.what()); }
        catch (...) { error = Problem(ErrorCode::TaskFailed, "ExecuteTask", "The batch failed with a non-standard exception"); }
        FinishBatch(node, std::move(error));
    }
    void Timeline::Impl::FinishBatch(Node& node, std::optional<Error> error) noexcept {
        bool last = false;
        {
            std::lock_guard lock(GraphMutex);
            if (error && !node.FirstError) node.FirstError = std::move(error);
            last = --node.Children == 0;
        }
        if (!last) return;
        auto task = node.Owner->shared_from_this();
        auto completed = task->Prepared->CompleteChunk(node.Chunks.front());
        if (!completed && !node.FirstError) node.FirstError = completed.GetError();
        CompleteNode(node, std::move(node.FirstError));
    }
    void Timeline::Impl::CompleteNode(Node& node, std::optional<Error> error) noexcept {
        auto task = node.Owner->shared_from_this();
        if (error) error->Task = task->Completion->Sequence;
        bool final = false;
        {
            std::lock_guard lock(GraphMutex);
            if (node.Completed) return;
            if (error && !task->FirstFailure) task->FirstFailure = std::move(error);
            if (task->FirstFailure && task->FirstFailure->Code != ErrorCode::Cancelled && !Failure) Failure = task->FirstFailure;
            CompleteSignalLocked(node, task->FirstFailure.has_value());
            final = --task->RemainingNodes == 0;
        }
        if (final) FinalizeTask(*task);
    }
    void Timeline::Impl::FinalizeTask(Task& task) noexcept {
        auto owned = task.shared_from_this();
        try {
            auto result = task.Prepared->Finish(!task.FirstFailure);
            if (!result && !task.FirstFailure) task.FirstFailure = result.GetError();
        }
        catch (const std::exception& exception) {
            if (!task.FirstFailure) task.FirstFailure = Problem(ErrorCode::TaskFailed, "FinishTask", exception.what());
        }
        catch (...) {
            if (!task.FirstFailure) task.FirstFailure = Problem(ErrorCode::TaskFailed, "FinishTask", "The task failed while finishing");
        }
        task.Prepared.reset();
        task.Submission.Release();
        task.Callback.reset();
        task.Dependencies.clear();
        {
            std::lock_guard lock(GraphMutex);
            if (task.FirstFailure) {
                task.FirstFailure->Task = task.Completion->Sequence;
                task.Completion->Failure = task.FirstFailure;
                if (task.FirstFailure->Code != ErrorCode::Cancelled && !Failure) Failure = task.FirstFailure;
                task.Completion->Status.store(task.FirstFailure->Code == ErrorCode::Cancelled ? TaskStatus::Cancelled : TaskStatus::Failed, std::memory_order_release);
            }
            else task.Completion->Status.store(TaskStatus::Succeeded, std::memory_order_release);
            CompleteSignalLocked(*task.Aggregate, task.FirstFailure.has_value());
            if (Context.m_ScheduledWork.fetch_sub(1, std::memory_order_release) == 1) {
                LocalFrontiers.clear();
                RandomFrontiers.clear();
                ResourceFrontiers.clear();
                ConsumerFrontiers.clear();
            }
        }
        GraphWake.notify_all();
        FinishClosing();
    }
    Result<void> Timeline::Impl::WaitParallel(const std::shared_ptr<Detail::TaskCompletion>& completion) {
        std::unique_lock lock(GraphMutex);
        GraphWake.wait(lock, [&] {
            return Complete(completion->Status.load(std::memory_order_acquire)) &&
                !Context.m_ClosingTimeline.load(std::memory_order_acquire);
        });
        if (completion->Failure) return *completion->Failure;
        return {};
    }
    Result<void> Timeline::Impl::DrainParallel() {
        std::unique_lock lock(GraphMutex);
        GraphWake.wait(lock, [&] {
            return Context.m_ScheduledWork.load(std::memory_order_acquire) == 0 &&
                !Context.m_ClosingTimeline.load(std::memory_order_acquire);
        });
        if (Failure) return *Failure;
        return {};
    }
    void Timeline::Impl::RetireParallel() {
        std::lock_guard lock(GraphMutex);
        Retire();
    }
    std::optional<Error> Timeline::Impl::FailureSnapshot() {
        if (!Parallel()) return Failure;
        std::lock_guard lock(GraphMutex);
        return Failure;
    }

    Timeline::Timeline(EcsContext& context, TimelineOptions options) {
        if (!context.IsMainThread()) throw std::logic_error("Timeline must be constructed on the Context main thread");
        if (context.TimelineSnapshot() || context.m_ClosingTimeline.load() || context.HasScheduledWork()) throw std::logic_error("This Context already has an active Timeline");
        if (!options.RowsPerBatch) throw std::invalid_argument("RowsPerBatch must be positive");
        m_Impl = std::make_shared<Impl>(context, options);
        if (!m_Impl->Identity) throw std::length_error("Timeline identity space is exhausted");
        {
            std::lock_guard lock(context.m_TimelineMutex);
            context.m_TimelineControl = m_Impl;
        }
        if (m_Impl->Parallel()) {
            auto started = m_Impl->StartRunners();
            if (!started) {
                std::lock_guard lock(context.m_TimelineMutex);
                context.m_TimelineControl.reset();
                throw std::runtime_error(started.GetError().Message);
            }
        }
    }
    Timeline::~Timeline() {
        if (!m_Impl) return;
        auto state = m_Impl;
        if (state->Executing.load() || (state->Parallel() && !state->Context.IsMainThread())) {
            state->Closing = true;
            state->Context.m_ClosingTimeline.store(true, std::memory_order_release);
            state->FinishClosing();
            return;
        }
        try { (void)Finish(); } catch (...) {}
        if (state->Parallel()) state->StopRunners();
        state->DiscardCommands();
        for (auto& task : state->Tasks) {
            if (!Complete(task->Completion->Status.load())) {
                task->Submission.Release();
                task->Callback.reset();
                task->Completion->Status.store(TaskStatus::Cancelled, std::memory_order_release);
                state->Context.m_ScheduledWork.fetch_sub(1, std::memory_order_release);
            }
        }
        {
            std::lock_guard lock(state->Context.m_TimelineMutex);
            if (state->Context.m_TimelineControl.get() == state.get()) state->Context.m_TimelineControl.reset();
        }
        state->Context.m_ClosingTimeline.store(false, std::memory_order_release);
    }
    Result<TaskHandle> Timeline::SubmitOwned(Query& query, World& world, std::unique_ptr<CallbackBase> callback, ChangedState* changed) {
        auto state = m_Impl;
        if (auto ready = state->Check("Submit"); !ready) return ready.GetError();
        if (auto failure = state->FailureSnapshot()) return *failure;
        if (&world.Context() != &state->Context) return Problem(ErrorCode::InvalidArgument, "Submit", "The World belongs to another Context");
        if (state->NextSequence == std::numeric_limits<uint64_t>::max()) return Problem(ErrorCode::InvalidState, "Submit", "Task sequence space is exhausted");
        auto captured = Detail::QuerySubmission::Capture(query, world, changed);
        if (!captured) return captured.GetError();
        auto task = std::make_shared<Impl::Task>();
        task->Submission = std::move(captured).Value();
        task->Callback = std::move(callback);
        task->MainId = task->Submission.MainId();
        task->MainLifetime = task->Submission.MainLifetime();
        task->Worlds.push_back(task->MainId);
        for (const auto& access : task->Submission.RandomAccesses()) task->Worlds.push_back(access.World);
        task->Random.assign(task->Submission.RandomAccesses().begin(), task->Submission.RandomAccesses().end());
        task->Resources.assign(task->Submission.ResourceAccesses().begin(), task->Submission.ResourceAccesses().end());
        task->Consumer = task->Submission.ConsumerId();
        std::sort(task->Worlds.begin(), task->Worlds.end());
        task->Worlds.erase(std::unique(task->Worlds.begin(), task->Worlds.end()), task->Worlds.end());
        task->Completion = std::make_shared<Detail::TaskCompletion>();
        task->Completion->Timeline = state->Identity;
        task->Completion->Sequence = state->NextSequence;
        auto prepared = task->Submission.Prepare({state->Options.RowsPerBatch, state->Options.SmallTaskRows});
        if (!prepared) return prepared.GetError();
        task->Prepared.emplace(std::move(prepared).Value());
        if (state->Parallel()) {
            TaskHandle result(task->Completion);
            auto submitted = state->SubmitParallel(std::move(task));
            if (!submitted) return submitted.GetError();
            return result;
        }
        for (const auto& previous : state->Tasks) {
            if (!Complete(previous->Completion->Status.load()) && Impl::Depends(task->Submission, previous->Submission)) task->Dependencies.push_back(previous->Completion);
        }
        TaskHandle result(task->Completion);
        state->Tasks.push_back(std::move(task));
        ++state->NextSequence;
        state->Context.m_ScheduledWork.fetch_add(1, std::memory_order_release);
        return result;
    }
    Result<void> Timeline::Wait(const TaskHandle& task) {
        auto state = m_Impl;
        if (auto ready = state->Check("Wait"); !ready) return ready;
        // The callback may clear the caller's handle while this wait is running.
        auto completion = task.m_Completion;
        if (!completion || completion->Timeline != state->Identity) return Problem(ErrorCode::InvalidArgument, "Wait", "The task handle does not belong to this Timeline");
        if (state->Parallel()) {
            auto result = state->WaitParallel(completion);
            state->RetireParallel();
            return result;
        }
        state->RunThrough(completion->Sequence);
        state->Retire();
        if (completion->Failure) return *completion->Failure;
        return {};
    }
    Result<void> Timeline::Finish() {
        auto state = m_Impl;
        if (auto ready = state->Check("Finish"); !ready) return ready;
        if (state->Parallel()) {
            auto result = state->DrainParallel();
            if (!result) state->DiscardCommands();
            state->RetireParallel();
            state->FinishClosing();
            return result;
        }
        state->RunThrough(std::numeric_limits<uint64_t>::max());
        if (state->Failure) {
            state->DiscardCommands();
            state->Retire();
            state->FinishClosing();
            return *state->Failure;
        }
        state->Retire();
        state->FinishClosing();
        return {};
    }
    Result<void> Timeline::CommitCommands() {
        auto state = m_Impl;
        if (auto ready = state->Check("CommitCommands"); !ready) return ready;
        if (state->Parallel()) (void)state->DrainParallel();
        else state->RunThrough(std::numeric_limits<uint64_t>::max());
        if (state->Failure) {
            state->DiscardCommands();
            state->Retire();
            state->FinishClosing();
            return *state->Failure;
        }
        if (state->Closed) return {};
        ExecutionGuard commitGuard(state->Executing);
        for (const auto& task : state->Tasks) {
            if (std::none_of(task->Commands.begin(), task->Commands.end(), [](const auto& batch) {
                return !batch.Buffer->Consumed() && batch.Buffer->CommandCount() != 0;
            })) continue;
            const auto lifetime = task->MainLifetime.lock();
            const auto* world = lifetime ? lifetime->Owner.load(std::memory_order_acquire) : nullptr;
            if (world) {
                auto access = world->CheckEditAccess();
                if (!access) return access;
            }
        }
        for (auto& task : state->Tasks) {
            std::sort(task->Commands.begin(), task->Commands.end(), [](const auto& a, const auto& b) {
                return std::tie(a.Chunk, a.Index) < std::tie(b.Chunk, b.Index);
            });
            for (auto& batch : task->Commands) {
                if (batch.Buffer->Consumed() || batch.Buffer->CommandCount() == 0) continue;
                const auto lifetime = task->MainLifetime.lock();
                auto* world = lifetime ? lifetime->Owner.load(std::memory_order_acquire) : nullptr;
                Result<void> result = world ? batch.Buffer->Playback(*world) : Result<void>(Problem(ErrorCode::InvalidState, "CommitCommands", "The command target World was destroyed"));
                if (!result) {
                    auto error = std::move(result.GetError());
                    error.Task = task->Completion->Sequence;
                    error.Message = "Chunk " + std::to_string(batch.Chunk.Id) + ", batch " + std::to_string(batch.Index) + ": " + error.Message;
                    state->Failure = error;
                    state->DiscardCommands();
                    state->Retire();
                    state->FinishClosing();
                    return error;
                }
            }
        }
        state->Retire();
        state->FinishClosing();
        return {};
    }
    Result<void> Timeline::ResetError() {
        auto state = m_Impl;
        if (auto ready = state->Check("ResetError"); !ready) return ready;
        if (state->Context.HasScheduledWork()) return Problem(ErrorCode::Busy, "ResetError", "All tasks must finish before clearing the error");
        if (!state->FailureSnapshot()) return {};
        state->DiscardCommands();
        if (state->Parallel()) {
            state->RetireParallel();
            std::lock_guard lock(state->GraphMutex);
            state->Failure.reset();
        }
        else {
            state->Retire();
            state->Failure.reset();
        }
        state->FinishClosing();
        return {};
    }
    Result<EntityId> Timeline::Resolve(const TaskHandle& task, TemporaryEntity entity) const {
        if (auto ready = m_Impl->Check("Resolve"); !ready) return ready.GetError();
        if (!task.m_Completion || task.m_Completion->Timeline != m_Impl->Identity) return Problem(ErrorCode::InvalidArgument, "Resolve", "The task handle does not belong to this Timeline");
        if (m_Impl->Parallel() && !Complete(task.m_Completion->Status.load(std::memory_order_acquire))) {
            return Problem(ErrorCode::Busy, "Resolve", "The task is still recording commands");
        }
        for (const auto& buffer : task.m_Completion->Mappings) {
            auto result = buffer->Resolve(entity);
            if (result || result.GetError().Code != ErrorCode::InvalidEntity) return result;
        }
        return Problem(ErrorCode::InvalidEntity, "Resolve", "The temporary entity is not owned by this task");
    }
    std::optional<Error> Timeline::LastError() const {
        if (!m_Impl->Context.IsMainThread()) return Problem(ErrorCode::WrongThread, "LastError", "Timeline error inspection requires the Context main thread");
        return m_Impl->FailureSnapshot();
    }
    bool Timeline::IsIdle() const noexcept { return !m_Impl->Context.HasScheduledWork(); }
    Result<void> Timeline::Synchronize(World& world) {
        return m_Impl->SynchronizeWorld(world);
    }
    void Timeline::OnWorldDestroy(World& world) noexcept {
        m_Impl->WorldDestroyed(world);
    }
}
