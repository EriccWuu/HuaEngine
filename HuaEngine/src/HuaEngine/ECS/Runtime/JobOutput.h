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
        struct JobOutputState final {
            std::mutex Mutex;
            std::map<size_t, std::vector<T>> Batches;
            bool Collected = false;

            [[nodiscard]] Result<void> Publish(size_t batch, std::vector<T>&& values) {
                std::lock_guard lock(Mutex);
                if (Collected || Batches.contains(batch))
                    return Error{ErrorCode::InvalidState, "JobOutput", "The output batch was already published or consumed"};
                Batches.emplace(batch, std::move(values));
                return {};
            }
        };
    }

    // Output belongs to this submission. Collect submissions in submission order
    // when joining several tasks; each task merges its batches in BatchIndex order.
    template<typename T>
    class OutputTask final {
    public:
        OutputTask(TaskHandle task, std::shared_ptr<Detail::JobOutputState<T>> output)
            : m_Task(std::move(task)), m_Output(std::move(output)) {}
        OutputTask(const OutputTask&) = delete;
        OutputTask& operator=(const OutputTask&) = delete;
        OutputTask(OutputTask&&) noexcept = default;
        OutputTask& operator=(OutputTask&&) noexcept = default;
        [[nodiscard]] const TaskHandle& Task() const noexcept { return m_Task; }

        [[nodiscard]] Result<std::vector<T>> Collect(Timeline& timeline) {
            // Waiting can invoke user code that destroys the public task object.
            auto output = m_Output;
            auto task = m_Task;
            if (!output) return Error{ErrorCode::InvalidState, "JobOutput", "The output task was moved from"};
            auto completion = timeline.Wait(task);
            if (!completion) return completion.GetError();
            if (task.Status() != TaskStatus::Succeeded)
                return Error{ErrorCode::InvalidState, "JobOutput", "Only a successful task can produce output"};
            std::map<size_t, std::vector<T>> batches;
            {
                std::lock_guard lock(output->Mutex);
                if (output->Collected)
                    return Error{ErrorCode::InvalidState, "JobOutput", "Output can only be collected once"};
                output->Collected = true;
                batches.swap(output->Batches);
            }
            try {
                std::vector<T> result;
                size_t total = 0;
                for (const auto& [index, values] : batches) {
                    if (values.size() > result.max_size() - total)
                        throw std::length_error("Job output exceeds the vector capacity");
                    total += values.size();
                }
                result.reserve(total);
                for (auto& [index, values] : batches)
                    for (auto& value : values) result.push_back(std::move(value));
                return result;
            } catch (const std::exception& exception) {
                return Error{ErrorCode::ConstructionFailed, "JobOutput", exception.what()};
            } catch (...) {
                return Error{ErrorCode::ConstructionFailed, "JobOutput", "Output collection failed"};
            }
        }
    private:
        TaskHandle m_Task;
        std::shared_ptr<Detail::JobOutputState<T>> m_Output;
    };
}
