#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace HE::Ecs {
    enum class ErrorCode {
        InvalidType, DuplicateType, UnsupportedOperation, ConstructionFailed,
        InvalidEntity, InvalidArgument, WrongThread, Busy, InvalidState,
        TaskFailed, Cancelled, CommandFailed
    };

    struct Error {
        ErrorCode Code = ErrorCode::InvalidState;
        std::string Operation;
        std::string Message;
        uint64_t Task = 0;
        uint64_t Command = 0;
    };

    template<typename T>
    class [[nodiscard]] Result {
    public:
        Result(T value) : m_Value(std::move(value)) {}
        Result(Error error) : m_Value(std::move(error)) {}

        [[nodiscard]] bool HasValue() const noexcept { return std::holds_alternative<T>(m_Value); }
        explicit operator bool() const noexcept { return HasValue(); }
        T& Value() & { Check(); return std::get<T>(m_Value); }
        const T& Value() const& { Check(); return std::get<T>(m_Value); }
        T&& Value() && { Check(); return std::get<T>(std::move(m_Value)); }
        [[nodiscard]] const Error& GetError() const { return std::get<Error>(m_Value); }

    private:
        void Check() const {
            if (!HasValue()) {
                const auto& error = GetError();
                throw std::runtime_error(error.Operation + ": " + error.Message);
            }
        }
        std::variant<T, Error> m_Value;
    };

    template<>
    class [[nodiscard]] Result<void> {
    public:
        Result() = default;
        Result(Error error) : m_Error(std::move(error)) {}
        [[nodiscard]] bool HasValue() const noexcept { return !m_Error.has_value(); }
        explicit operator bool() const noexcept { return HasValue(); }
        void Value() const {
            if (m_Error) throw std::runtime_error(m_Error->Operation + ": " + m_Error->Message);
        }
        [[nodiscard]] const Error& GetError() const { return m_Error.value(); }
    private:
        std::optional<Error> m_Error;
    };
}
