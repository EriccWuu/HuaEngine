#pragma once

#include "HuaEngine/ECS/Runtime/World.h"

#include <optional>
#include <stdexcept>

namespace HE::Ecs {
    class QueryBatch;
    namespace Detail {
        struct QueryBatchData;
        struct ColumnSpan { void* Data = nullptr; size_t Count = 0; };
        [[nodiscard]] bool BatchValid(const std::weak_ptr<QueryBatchData>& batch) noexcept;
        [[nodiscard]] size_t BatchSize(const std::weak_ptr<QueryBatchData>& batch) noexcept;
        [[nodiscard]] Result<void> ValidateColumn(const std::weak_ptr<QueryBatchData>& batch,
            size_t argument, const void* nativeKey, bool write);
        [[nodiscard]] Result<void*> ColumnPointer(const std::weak_ptr<QueryBatchData>& batch,
            size_t argument, size_t row, const void* nativeKey, bool write);
        [[nodiscard]] std::optional<ColumnSpan> ContiguousColumn(const std::weak_ptr<QueryBatchData>& batch,
            size_t argument, const void* nativeKey, bool write);
    }

    // Logical rows follow query masks. Native references and spans cannot escape the batch.
    template<typename T>
    class ColumnView {
    public:
        ColumnView() = default;
        [[nodiscard]] bool Valid() const noexcept { return Detail::BatchValid(m_Batch); }
        [[nodiscard]] size_t Size() const noexcept { return Detail::BatchSize(m_Batch); }
        [[nodiscard]] T* TryGet(size_t row) const {
            auto pointer = Detail::ColumnPointer(m_Batch, m_Argument, row,
                NativeTypeKey<std::remove_const_t<T>>(), !std::is_const_v<T>);
            return pointer ? static_cast<T*>(pointer.Value()) : nullptr;
        }
        [[nodiscard]] bool Has(size_t row) const { return TryGet(row) != nullptr; }
        [[nodiscard]] T& At(size_t row) const {
            auto pointer = Detail::ColumnPointer(m_Batch, m_Argument, row,
                NativeTypeKey<std::remove_const_t<T>>(), !std::is_const_v<T>);
            if (!pointer) throw std::logic_error(pointer.GetError().Message);
            if (!pointer.Value()) throw std::logic_error("The optional component is absent from this row");
            return *static_cast<T*>(pointer.Value());
        }
        [[nodiscard]] T& operator[](size_t row) const { return At(row); }
        [[nodiscard]] std::optional<std::span<T>> TryAsSpan() const {
            auto span = Detail::ContiguousColumn(m_Batch, m_Argument,
                NativeTypeKey<std::remove_const_t<T>>(), !std::is_const_v<T>);
            if (!span) return std::nullopt;
            return std::span<T>(static_cast<T*>(span->Data), span->Count);
        }

    private:
        friend class QueryBatch;
        ColumnView(std::weak_ptr<Detail::QueryBatchData> batch, size_t argument)
            : m_Batch(std::move(batch)), m_Argument(argument) {}
        std::weak_ptr<Detail::QueryBatchData> m_Batch;
        size_t m_Argument = 0;
    };
}
