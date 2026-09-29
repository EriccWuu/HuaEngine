#pragma once

#include "HuaEngine/ECS/Runtime/GeneratedQuery.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace HE::Ecs {
    template<typename T>
    using ComponentView = ColumnView<T>;

    namespace Detail {
        struct JobBinding final {
            std::function<Result<void>(void*, QueryBatch&)> Bind;
        };
        struct JobAccess final {
            QuerySpec Spec;
            std::vector<JobBinding> Bindings;
        };
    }

    class AccessBuilder final {
    public:
        AccessBuilder(EcsContext& context, const void* jobType) noexcept
            : m_Types(context.Types()), m_JobType(jobType) {}

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
        [[nodiscard]] Result<Detail::JobAccess> Finish() && {
            if (m_Error) return *m_Error;
            return std::move(m_Access);
        }

    private:
        template<typename Job, typename T>
        AccessBuilder& AddMember(ComponentView<T> Job::* member, Presence presence, AccessMode mode) {
            if (m_Error) return *this;
            if (NativeTypeKey<Job>() != m_JobType) {
                m_Error = Error{ErrorCode::InvalidArgument, "Dispatch", "A Job view belongs to another Job type"};
                return *this;
            }
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
        template<typename T> AccessBuilder& AddFilter(std::vector<TypeId>& destination) {
            if (m_Error) return *this;
            const auto* type = m_Types.Find<std::remove_cv_t<T>>();
            if (!type) m_Error = Error{ErrorCode::InvalidType, "Dispatch", "A Job filter type is not registered"};
            else destination.push_back(type->Id);
            return *this;
        }
        TypeRegistry& m_Types;
        const void* m_JobType;
        Detail::JobAccess m_Access;
        std::optional<Error> m_Error;
    };

    class TaskContext final {
    public:
        [[nodiscard]] size_t size() const noexcept { return m_Batch.View().Size(); }
        [[nodiscard]] EntityId entity(size_t row) const noexcept { return m_Batch.View().Entity(row); }
        [[nodiscard]] CommandBuffer& commands() const noexcept { return m_Batch.Commands(); }

    private:
        friend class Timeline;
        explicit TaskContext(TaskBatch& batch) noexcept : m_Batch(batch) {}
        TaskBatch& m_Batch;
    };

    namespace Detail {
        template<typename Job>
        [[nodiscard]] Result<std::pair<Query*, std::shared_ptr<const std::vector<JobBinding>>>> PrepareJob(
            EcsContext& context, Job& job) {
            AccessBuilder builder(context, NativeTypeKey<std::remove_cvref_t<Job>>());
            if constexpr (std::is_void_v<decltype(job.build(builder))>) job.build(builder);
            else {
                auto built = job.build(builder);
                if (!built) return built.GetError();
            }
            auto access = std::move(builder).Finish();
            if (!access) return access.GetError();
            auto bindings = std::make_shared<const std::vector<JobBinding>>(std::move(access.Value().Bindings));
            std::string key = "job:";
            key += std::to_string(reinterpret_cast<uintptr_t>(NativeTypeKey<std::remove_cvref_t<Job>>()));
            auto query = context.FindOrCreateGeneratedQuery(key, std::move(access).Value().Spec);
            if (!query) return query.GetError();
            return std::pair{query.Value(), std::move(bindings)};
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
            auto prepared = Detail::PrepareJob(world.Context(), job);
            if (!prepared) return prepared.GetError();
            auto prototype = std::make_shared<const Owned>(std::forward<Job>(job));
            auto bindings = std::move(prepared.Value().second);
            return Submit(*prepared.Value().first, world,
                [prototype = std::move(prototype), bindings = std::move(bindings)](TaskBatch& batch) -> Result<void> {
                    Owned local(*prototype);
                    auto bound = Detail::BindJob(local, batch, *bindings);
                    if (!bound) return bound;
                    TaskContext context(batch);
                    return local.run(context);
                });
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
            auto prepared = Detail::PrepareJob(world.Context(), job);
            if (!prepared) return prepared.GetError();
            auto prototype = std::make_shared<const Owned>(std::forward<Job>(job));
            auto state = std::make_shared<Detail::GeneratedOutputState<Output>>();
            auto bindings = std::move(prepared.Value().second);
            auto submitted = Submit(*prepared.Value().first, world,
                [prototype = std::move(prototype), bindings = std::move(bindings), state](TaskBatch& batch) -> Result<void> {
                    Owned local(*prototype);
                    auto bound = Detail::BindJob(local, batch, *bindings);
                    if (!bound) return bound;
                    TaskContext context(batch);
                    std::vector<Output> values;
                    BatchOutput<Output> output(values);
                    auto result = local.run(context, output);
                    if (!result) return result;
                    return state->Publish(batch.BatchIndex(), std::move(values));
                });
            if (!submitted) return submitted.GetError();
            return OutputTask<Output>(std::move(submitted).Value(), std::move(state));
        } catch (const std::exception& exception) {
            return Error{ErrorCode::ConstructionFailed, "Dispatch", exception.what()};
        } catch (...) {
            return Error{ErrorCode::ConstructionFailed, "Dispatch", "Job construction failed"};
        }
    }
}
