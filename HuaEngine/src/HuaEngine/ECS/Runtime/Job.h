#pragma once

#include "HuaEngine/ECS/Runtime/JobOutput.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace HE::Ecs {
    template<typename T>
    using ComponentView = ColumnView<T>;

    template<typename T>
    class ResourceView final {
    public:
        ResourceView() = default;
        ResourceView(QueryBatch batch, ResourceHandle handle) noexcept
            : m_Batch(std::move(batch)), m_Handle(handle) {}
        [[nodiscard]] Result<std::reference_wrapper<T>> TryGet() const {
            return m_Batch.Resource<T>(m_Handle);
        }
        [[nodiscard]] T& Get() const {
            auto value = TryGet();
            if (!value) throw std::logic_error(value.GetError().Message);
            return value.Value().get();
        }
        [[nodiscard]] T& operator*() const { return Get(); }
        [[nodiscard]] T* operator->() const { return std::addressof(Get()); }

    private:
        QueryBatch m_Batch;
        ResourceHandle m_Handle;
    };

    namespace Detail {
        struct JobBinding final {
            std::function<Result<void>(void*, QueryBatch&)> Bind;
        };
        struct JobAccess final {
            QuerySpec Spec;
            std::vector<JobBinding> Bindings;
            ChangedState* Changed = nullptr;
        };
    }

    class AccessBuilder final {
    public:
        AccessBuilder(World& world, const void* jobType) noexcept
            : m_World(world), m_Context(world.Context()), m_Types(m_Context.Types()), m_JobType(jobType) {}

        template<typename Job, typename T>
        AccessBuilder& read(ComponentView<T> Job::* member) {
            static_assert(std::is_const_v<T>, "A read view must have a const component type");
            return AddMember(member, Presence::Required, AccessMode::Read);
        }
        template<typename Job, typename T>
        AccessBuilder& write(ComponentView<T> Job::* member) {
            static_assert(!std::is_const_v<T>, "A write view must have a mutable component type");
            return AddMember(member, Presence::Required, AccessMode::Write);
        }
        template<typename Job, typename T>
        AccessBuilder& optional_read(ComponentView<T> Job::* member) {
            static_assert(std::is_const_v<T>, "An optional read view must have a const component type");
            return AddMember(member, Presence::Optional, AccessMode::Read);
        }
        template<typename Job, typename T>
        AccessBuilder& optional_write(ComponentView<T> Job::* member) {
            static_assert(!std::is_const_v<T>, "An optional write view must have a mutable component type");
            return AddMember(member, Presence::Optional, AccessMode::Write);
        }
        template<typename T> AccessBuilder& has() { return AddFilter<T>(m_Access.Spec.Required); }
        template<typename T> AccessBuilder& none() { return AddFilter<T>(m_Access.Spec.Exclude); }
        AccessBuilder& include_disabled() noexcept {
            m_Access.Spec.IncludeDisabledEntities = true;
            return *this;
        }
        AccessBuilder& ignore_component_enabled() noexcept {
            m_Access.Spec.IgnoreComponentEnabled = true;
            return *this;
        }
        template<typename T>
        AccessBuilder& shared(ResourceHandle handle) {
            if (m_Error) return *this;
            const auto* type = m_Types.Find<std::remove_cv_t<T>>();
            const auto* resource = m_Context.Resources().Find(handle);
            if (!type || type->Descriptor.Storage == StorageKind::Tag || !resource ||
                resource->NativeKey != NativeTypeKey<std::remove_cv_t<T>>()) {
                m_Error = Error{ErrorCode::InvalidType, "Dispatch", "A shared Job component or resource is not registered with this Context and type"};
            } else {
                m_Access.Spec.SharedBindings.push_back({type->Id, handle});
            }
            return *this;
        }
        template<typename Job, typename T>
        AccessBuilder& access(RandomView<T> Job::* member) {
            return AddRandom(member, m_World);
        }
        template<typename Job, typename T>
        AccessBuilder& access(RandomView<T> Job::* member, World& target) {
            return AddRandom(member, target);
        }
        template<typename Job, typename T>
        AccessBuilder& access(ResourceView<T> Job::* member, ResourceHandle handle) {
            if (m_Error) return *this;
            if (!MemberBelongsToJob<Job>()) return *this;
            const auto* resource = m_Context.Resources().Find(handle);
            if (!resource || resource->NativeKey != NativeTypeKey<std::remove_const_t<T>>()) {
                m_Error = Error{ErrorCode::InvalidType, "Dispatch", "A Job resource is not registered with this Context and type"};
                return *this;
            }
            m_Access.Spec.ResourceAccesses.push_back({handle,
                std::is_const_v<T> ? AccessMode::Read : AccessMode::Write});
            m_Access.Bindings.push_back({[member, handle](void* object, QueryBatch& batch) -> Result<void> {
                auto resource = batch.Resource<T>(handle);
                if (!resource) return resource.GetError();
                static_cast<Job*>(object)->*member = ResourceView<T>(batch, handle);
                return {};
            }});
            return *this;
        }
        template<typename T>
        AccessBuilder& changed(ChangedState& observer) {
            if (m_Error) return *this;
            const auto* type = m_Types.Find<std::remove_cv_t<T>>();
            if (!type || type->Descriptor.Storage == StorageKind::Tag) {
                m_Error = Error{ErrorCode::InvalidType, "Dispatch", "Changed requires a registered stored component"};
            } else if (m_Access.Changed && m_Access.Changed != &observer) {
                m_Error = Error{ErrorCode::InvalidArgument, "Dispatch", "A Job can use only one Changed observer"};
            } else {
                m_Access.Changed = &observer;
                m_Access.Spec.ChangedTypes.push_back(type->Id);
            }
            return *this;
        }
        [[nodiscard]] Result<Detail::JobAccess> Finish() && {
            if (m_Error) return *m_Error;
            return std::move(m_Access);
        }

    private:
        template<typename Job>
        bool MemberBelongsToJob() {
            if (NativeTypeKey<Job>() == m_JobType) return true;
            m_Error = Error{ErrorCode::InvalidArgument, "Dispatch", "A Job view belongs to another Job type"};
            return false;
        }
        template<typename Job, typename T>
        AccessBuilder& AddMember(ComponentView<T> Job::* member, Presence presence, AccessMode mode) {
            if (m_Error) return *this;
            if (!MemberBelongsToJob<Job>()) return *this;
            const auto* type = m_Types.Find<std::remove_const_t<T>>();
            if (!type) {
                m_Error = Error{ErrorCode::InvalidType, "Dispatch", "A Job component is not registered"};
                return *this;
            }
            if (type->Descriptor.Storage == StorageKind::Tag) {
                m_Error = Error{ErrorCode::InvalidType, "Dispatch", "A tag cannot be used as a Job column"};
                return *this;
            }
            const size_t index = m_Access.Spec.Columns.size();
            m_Access.Spec.Columns.push_back({type->Id, presence, mode});
            m_Access.Bindings.push_back({[member, index](void* object, QueryBatch& batch) -> Result<void> {
                auto column = batch.Column<T>(index);
                if (!column) return column.GetError();
                static_cast<Job*>(object)->*member = std::move(column).Value();
                return {};
            }});
            return *this;
        }
        template<typename Job, typename T>
        AccessBuilder& AddRandom(RandomView<T> Job::* member, World& target) {
            if (m_Error) return *this;
            if (!MemberBelongsToJob<Job>()) return *this;
            if (&target.Context() != &m_Context) {
                m_Error = Error{ErrorCode::InvalidArgument, "Dispatch", "A Job random-access World belongs to another Context"};
                return *this;
            }
            const auto* type = m_Types.Find<std::remove_const_t<T>>();
            if (!type || type->Descriptor.Storage == StorageKind::Tag) {
                m_Error = Error{ErrorCode::InvalidType, "Dispatch", "Random access requires a registered stored component"};
                return *this;
            }
            m_Access.Spec.RandomAccesses.push_back({&target, type->Id,
                std::is_const_v<T> ? AccessMode::Read : AccessMode::Write});
            const WorldId targetId = target.Id();
            m_Access.Bindings.push_back({[member, targetId](void* object, QueryBatch& batch) -> Result<void> {
                auto random = batch.Random<T>(targetId);
                if (!random) return random.GetError();
                static_cast<Job*>(object)->*member = std::move(random).Value();
                return {};
            }});
            return *this;
        }
        template<typename T> AccessBuilder& AddFilter(std::vector<TypeId>& destination) {
            if (m_Error) return *this;
            const auto* type = m_Types.Find<std::remove_cv_t<T>>();
            if (!type) m_Error = Error{ErrorCode::InvalidType, "Dispatch", "A Job filter type is not registered"};
            else destination.push_back(type->Id);
            return *this;
        }
        World& m_World;
        EcsContext& m_Context;
        TypeRegistry& m_Types;
        const void* m_JobType;
        Detail::JobAccess m_Access;
        std::optional<Error> m_Error;
    };

    class TaskContext final {
    public:
        [[nodiscard]] size_t size() const noexcept { return m_Batch.View().Size(); }
        [[nodiscard]] bool valid() const noexcept { return m_Batch.View().Valid(); }
        [[nodiscard]] EntityId entity(size_t row) const noexcept { return m_Batch.View().Entity(row); }
        [[nodiscard]] CommandBuffer& commands() const noexcept { return m_Batch.Commands(); }

    private:
        friend class Timeline;
        explicit TaskContext(TaskBatch& batch) noexcept : m_Batch(batch) {}
        TaskBatch& m_Batch;
    };

    namespace Detail {
        struct PreparedJob final {
            Query* QueryInstance = nullptr;
            std::shared_ptr<const std::vector<JobBinding>> Bindings;
            ChangedState* Changed = nullptr;
        };
        template<typename Job>
        [[nodiscard]] Result<PreparedJob> PrepareJob(World& world, Job& job) {
            auto& context = world.Context();
            AccessBuilder builder(world, NativeTypeKey<std::remove_cvref_t<Job>>());
            if constexpr (std::is_void_v<decltype(job.build(builder))>) job.build(builder);
            else {
                auto built = job.build(builder);
                if (!built) return built.GetError();
            }
            auto access = std::move(builder).Finish();
            if (!access) return access.GetError();
            auto bindings = std::make_shared<const std::vector<JobBinding>>(std::move(access.Value().Bindings));
            auto* changed = access.Value().Changed;
            std::string key = "job:";
            key += std::to_string(reinterpret_cast<uintptr_t>(NativeTypeKey<std::remove_cvref_t<Job>>()));
            auto query = context.FindOrCreateQuery(key, std::move(access).Value().Spec);
            if (!query) return query.GetError();
            return PreparedJob{query.Value(), std::move(bindings), changed};
        }

        template<typename Job>
        [[nodiscard]] Result<void> BindJob(Job& job, TaskBatch& batch, std::span<const JobBinding> bindings) {
            for (const auto& binding : bindings) {
                auto result = binding.Bind(&job, batch.View());
                if (!result) return result;
            }
            return {};
        }
    }

    template<typename Job>
        requires (!requires { typename std::remove_cvref_t<Job>::Output; })
    Result<TaskHandle> Timeline::Dispatch(World& world, Job&& job) {
        using Owned = std::remove_cvref_t<Job>;
        static_assert(std::is_copy_constructible_v<Owned>, "A dispatched Job must be copy constructible");
        try {
            auto prepared = Detail::PrepareJob(world, job);
            if (!prepared) return prepared.GetError();
            auto prototype = std::make_shared<const Owned>(std::forward<Job>(job));
            auto bindings = std::move(prepared.Value().Bindings);
            return Submit(*prepared.Value().QueryInstance, world,
                [prototype = std::move(prototype), bindings = std::move(bindings)](TaskBatch& batch) -> Result<void> {
                    if (!batch.View().Valid()) return Error{ErrorCode::InvalidState, "Dispatch", "The Job World was destroyed"};
                    Owned local(*prototype);
                    auto bound = Detail::BindJob(local, batch, *bindings);
                    if (!bound) return bound;
                    TaskContext context(batch);
                    auto result = local.run(context);
                    if (!batch.View().Valid()) return Error{ErrorCode::InvalidState, "Dispatch", "The Job World was destroyed"};
                    return result;
                }, prepared.Value().Changed);
        } catch (const std::exception& exception) {
            return Error{ErrorCode::ConstructionFailed, "Dispatch", exception.what()};
        } catch (...) {
            return Error{ErrorCode::ConstructionFailed, "Dispatch", "Job construction failed"};
        }
    }

    template<typename Job>
        requires requires { typename std::remove_cvref_t<Job>::Output; }
    Result<OutputTask<typename std::remove_cvref_t<Job>::Output>> Timeline::Dispatch(World& world, Job&& job) {
        using Owned = std::remove_cvref_t<Job>;
        using Output = typename Owned::Output;
        static_assert(std::is_copy_constructible_v<Owned>, "A dispatched Job must be copy constructible");
        try {
            auto prepared = Detail::PrepareJob(world, job);
            if (!prepared) return prepared.GetError();
            auto prototype = std::make_shared<const Owned>(std::forward<Job>(job));
            auto state = std::make_shared<Detail::JobOutputState<Output>>();
            auto bindings = std::move(prepared.Value().Bindings);
            auto submitted = Submit(*prepared.Value().QueryInstance, world,
                [prototype = std::move(prototype), bindings = std::move(bindings), state](TaskBatch& batch) -> Result<void> {
                    if (!batch.View().Valid()) return Error{ErrorCode::InvalidState, "Dispatch", "The Job World was destroyed"};
                    Owned local(*prototype);
                    auto bound = Detail::BindJob(local, batch, *bindings);
                    if (!bound) return bound;
                    TaskContext context(batch);
                    std::vector<Output> values;
                    BatchOutput<Output> output(values);
                    auto result = local.run(context, output);
                    if (!result) return result;
                    if (!batch.View().Valid()) return Error{ErrorCode::InvalidState, "Dispatch", "The Job World was destroyed"};
                    return state->Publish(batch.BatchIndex(), std::move(values));
                }, prepared.Value().Changed);
            if (!submitted) return submitted.GetError();
            return OutputTask<Output>(std::move(submitted).Value(), std::move(state));
        } catch (const std::exception& exception) {
            return Error{ErrorCode::ConstructionFailed, "Dispatch", exception.what()};
        } catch (...) {
            return Error{ErrorCode::ConstructionFailed, "Dispatch", "Job construction failed"};
        }
    }
}
