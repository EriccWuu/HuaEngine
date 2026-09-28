#pragma once

#include "HuaEngine/ECS/Runtime/Timeline.h"

#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace HE::Ecs {
    // This call-local view cannot be retained by copying or moving it.
    template<typename T>
    class BatchOutput final {
    public:
        explicit BatchOutput(std::vector<T>& values) noexcept : m_Values(values) {}
        BatchOutput(const BatchOutput&) = delete;
        BatchOutput& operator=(const BatchOutput&) = delete;
        BatchOutput(BatchOutput&&) = delete;
        BatchOutput& operator=(BatchOutput&&) = delete;
        void Emit(T value) { m_Values.push_back(std::move(value)); }
    private:
        std::vector<T>& m_Values;
    };

    namespace Detail {
        template<typename T>
        struct GeneratedOutputState final {
            std::mutex Mutex;
            std::map<size_t, std::vector<T>> Batches;
            bool Collected = false;

            [[nodiscard]] Result<void> Publish(size_t batch, std::vector<T>&& values) {
                std::lock_guard lock(Mutex);
                if (Collected || Batches.contains(batch))
                    return Error{ErrorCode::InvalidState, "GeneratedOutput", "The output batch was already published or consumed"};
                Batches.emplace(batch, std::move(values));
                return {};
            }
        };
    }

    // Output belongs to this submission. Collect submissions in submission order
    // when joining several tasks; each task merges its batches in BatchIndex order.
    template<typename T>
    class GeneratedOutputTask final {
    public:
        GeneratedOutputTask(TaskHandle task, std::shared_ptr<Detail::GeneratedOutputState<T>> output)
            : m_Task(std::move(task)), m_Output(std::move(output)) {}
        GeneratedOutputTask(const GeneratedOutputTask&) = delete;
        GeneratedOutputTask& operator=(const GeneratedOutputTask&) = delete;
        GeneratedOutputTask(GeneratedOutputTask&&) noexcept = default;
        GeneratedOutputTask& operator=(GeneratedOutputTask&&) noexcept = default;
        [[nodiscard]] const TaskHandle& Task() const noexcept { return m_Task; }

        [[nodiscard]] Result<std::vector<T>> Collect(Timeline& timeline) {
            // Waiting can invoke user code that destroys the public task object.
            auto output = m_Output;
            auto task = m_Task;
            if (!output) return Error{ErrorCode::InvalidState, "GeneratedOutput", "The output task was moved from"};
            auto completion = timeline.Wait(task);
            if (!completion) return completion.GetError();
            if (task.Status() != TaskStatus::Succeeded)
                return Error{ErrorCode::InvalidState, "GeneratedOutput", "Only a successful task can produce output"};
            std::map<size_t, std::vector<T>> batches;
            {
                std::lock_guard lock(output->Mutex);
                if (output->Collected)
                    return Error{ErrorCode::InvalidState, "GeneratedOutput", "Output can only be collected once"};
                output->Collected = true;
                batches.swap(output->Batches);
            }
            try {
                std::vector<T> result;
                size_t total = 0;
                for (const auto& [index, values] : batches) {
                    if (values.size() > result.max_size() - total)
                        throw std::length_error("Generated output exceeds the vector capacity");
                    total += values.size();
                }
                result.reserve(total);
                for (auto& [index, values] : batches)
                    for (auto& value : values) result.push_back(std::move(value));
                return result;
            } catch (const std::exception& exception) {
                return Error{ErrorCode::ConstructionFailed, "GeneratedOutput", exception.what()};
            } catch (...) {
                return Error{ErrorCode::ConstructionFailed, "GeneratedOutput", "Output collection failed"};
            }
        }
    private:
        TaskHandle m_Task;
        std::shared_ptr<Detail::GeneratedOutputState<T>> m_Output;
    };

    template<typename T>
    class Value final {
    public:
        explicit Value(const T& value) noexcept : m_Value(std::addressof(value)) {}
        [[nodiscard]] const T& Get() const noexcept { return *m_Value; }
        [[nodiscard]] const T& operator*() const noexcept { return Get(); }
        [[nodiscard]] const T* operator->() const noexcept { return m_Value; }
    private:
        const T* m_Value;
    };

    template<typename T>
    class RandomRead final {
    public:
        explicit RandomRead(RandomView<const T> view) : m_View(std::move(view)) {}
        [[nodiscard]] Result<const T*> TryGet(EntityId entity) const { return m_View.TryGet(entity); }
    private:
        RandomView<const T> m_View;
    };

    template<typename T>
    class RandomWrite final {
    public:
        explicit RandomWrite(RandomView<T> view) : m_View(std::move(view)) {}
        [[nodiscard]] Result<T*> TryGet(EntityId entity) const { return m_View.TryGet(entity); }
    private:
        RandomView<T> m_View;
    };

    template<typename T>
    class ResourceRead final {
    public:
        explicit ResourceRead(const T& value) noexcept : m_Value(std::addressof(value)) {}
        [[nodiscard]] const T& Get() const noexcept { return *m_Value; }
        [[nodiscard]] const T& operator*() const noexcept { return Get(); }
        [[nodiscard]] const T* operator->() const noexcept { return m_Value; }
    private:
        const T* m_Value;
    };

    template<typename T>
    class ResourceWrite final {
    public:
        explicit ResourceWrite(T& value) noexcept : m_Value(std::addressof(value)) {}
        [[nodiscard]] T& Get() const noexcept { return *m_Value; }
        [[nodiscard]] T& operator*() const noexcept { return Get(); }
        [[nodiscard]] T* operator->() const noexcept { return m_Value; }
    private:
        T* m_Value;
    };
}
