#include "HuaEngine/ECS/Runtime/Commands.h"

#include <atomic>
#include <exception>
#include <limits>
#include <utility>

namespace HE::Ecs {
    namespace {
        uint64_t AllocateNamespace() noexcept {
            static std::atomic<uint64_t> next{1};
            uint64_t value = next.load(std::memory_order_relaxed);
            while (value != std::numeric_limits<uint64_t>::max()) {
                if (next.compare_exchange_weak(value, value + 1, std::memory_order_relaxed)) return value;
            }
            return 0;
        }

        Error RecordFailure(const char* message) {
            return Error{ErrorCode::ConstructionFailed, "RecordCommands", message};
        }
    }

    CommandBuffer::CommandBuffer(EcsContext& context) : m_Context(&context), m_Namespace(AllocateNamespace()) {}

    CommandBuffer::CommandBuffer(CommandBuffer&& other) noexcept { *this = std::move(other); }

    CommandBuffer& CommandBuffer::operator=(CommandBuffer&& other) noexcept {
        if (this == &other) return *this;
        m_Context = std::exchange(other.m_Context, nullptr);
        m_Namespace = std::exchange(other.m_Namespace, 0);
        m_Consumed = std::exchange(other.m_Consumed, true);
        m_RecordedCount = std::exchange(other.m_RecordedCount, 0);
        m_AppliedCount = std::exchange(other.m_AppliedCount, 0);
        m_Commands = std::move(other.m_Commands);
        m_Resolved = std::move(other.m_Resolved);
        return *this;
    }

    Result<void> CommandBuffer::CheckRecording() const {
        if (!m_Context || m_Namespace == 0 || m_Consumed) {
            return Error{ErrorCode::InvalidState, "RecordCommands", "The command buffer is consumed, moved from, or has no available identity"};
        }
        return {};
    }

    Result<void> CommandBuffer::CheckTarget(const CommandEntity& entity) const {
        auto ready = CheckRecording();
        if (!ready) return ready.GetError();
        if (const auto* temporary = std::get_if<TemporaryEntity>(&entity)) {
            if (temporary->Buffer != m_Namespace || temporary->Index >= m_Resolved.size()) {
                return Error{ErrorCode::InvalidEntity, "RecordCommands", "A temporary entity belongs to another buffer or was never recorded"};
            }
        }
        return {};
    }

    Result<void> CommandBuffer::Append(Command&& command) {
        try {
            m_Commands.push_back(std::move(command));
            ++m_RecordedCount;
            return {};
        }
        catch (const std::exception& exception) { return RecordFailure(exception.what()); }
        catch (...) { return RecordFailure("Recording the command failed"); }
    }

    Result<TemporaryEntity> CommandBuffer::AppendCreation(Command&& command) {
        static_assert(std::is_nothrow_move_constructible_v<Command>);
        if (m_Resolved.size() >= std::numeric_limits<uint32_t>::max()) {
            return Error{ErrorCode::InvalidState, "RecordCommands", "The temporary entity index space is exhausted"};
        }
        try {
            // Allocate both ledgers before publishing either half of a creation command.
            if (m_Commands.size() == m_Commands.capacity()) {
                m_Commands.reserve(m_Commands.capacity() ? m_Commands.capacity() + m_Commands.capacity() / 2 + 1 : 8);
            }
            if (m_Resolved.size() == m_Resolved.capacity()) {
                m_Resolved.reserve(m_Resolved.capacity() ? m_Resolved.capacity() + m_Resolved.capacity() / 2 + 1 : 8);
            }
            const auto index = static_cast<uint32_t>(m_Resolved.size());
            command.TemporaryIndex = index;
            m_Commands.push_back(std::move(command));
            m_Resolved.push_back({});
            ++m_RecordedCount;
            return TemporaryEntity{m_Namespace, index};
        }
        catch (const std::exception& exception) { return RecordFailure(exception.what()); }
        catch (...) { return RecordFailure("Recording entity creation failed"); }
    }

    Result<TemporaryEntity> CommandBuffer::CreateEmpty(std::string_view name, EntityUuid uuid) {
        auto ready = CheckRecording();
        if (!ready) return ready.GetError();
        try {
            Command command;
            command.Operation = Kind::Create;
            command.Name = name;
            command.Uuid = uuid;
            return AppendCreation(std::move(command));
        }
        catch (const std::exception& exception) { return RecordFailure(exception.what()); }
        catch (...) { return RecordFailure("Recording entity creation failed"); }
    }

    Result<TemporaryEntity> CommandBuffer::Clone(CommandEntity source, std::string_view name, EntityUuid uuid) {
        auto ready = CheckTarget(source);
        if (!ready) return ready.GetError();
        try {
            Command command;
            command.Operation = Kind::Clone;
            command.Entity = source;
            command.Name = name;
            command.Uuid = uuid;
            return AppendCreation(std::move(command));
        }
        catch (const std::exception& exception) { return RecordFailure(exception.what()); }
        catch (...) { return RecordFailure("Recording entity cloning failed"); }
    }

    Result<void> CommandBuffer::Destroy(CommandEntity entity) {
        auto ready = CheckTarget(entity);
        if (!ready) return ready.GetError();
        Command command;
        command.Operation = Kind::Destroy;
        command.Entity = entity;
        return Append(std::move(command));
    }

    Result<void> CommandBuffer::ClearWorld() {
        auto ready = CheckRecording();
        if (!ready) return ready.GetError();
        Command command;
        command.Operation = Kind::Clear;
        return Append(std::move(command));
    }

    Result<void> CommandBuffer::Set(CommandEntity entity, OwnedValue&& value) {
        auto ready = CheckTarget(entity);
        if (!ready) return ready.GetError();
        if (!value || !value.Type() || !m_Context->Types().Owns(*value.Type())) {
            return Error{ErrorCode::InvalidType, "RecordCommands", "The payload must belong to this buffer's Context"};
        }
        Command command;
        command.Operation = Kind::Set;
        command.Entity = entity;
        command.Value = std::move(value);
        return Append(std::move(command));
    }

    Result<void> CommandBuffer::AddDefault(CommandEntity entity, TypeId type) {
        auto ready = CheckTarget(entity);
        if (!ready) return ready.GetError();
        Command command;
        command.Operation = Kind::AddDefault;
        command.Entity = entity;
        command.Type = type;
        return Append(std::move(command));
    }

    Result<void> CommandBuffer::Remove(CommandEntity entity, TypeId type) {
        auto ready = CheckTarget(entity);
        if (!ready) return ready.GetError();
        Command command;
        command.Operation = Kind::Remove;
        command.Entity = entity;
        command.Type = type;
        return Append(std::move(command));
    }

    Result<void> CommandBuffer::SetTag(CommandEntity entity, TypeId type, bool present) {
        auto ready = CheckTarget(entity);
        if (!ready) return ready.GetError();
        Command command;
        command.Operation = Kind::Tag;
        command.Entity = entity;
        command.Type = type;
        command.Present = present;
        return Append(std::move(command));
    }

    Result<void> CommandBuffer::SetShared(CommandEntity entity, SharedBinding binding) {
        auto ready = CheckTarget(entity);
        if (!ready) return ready.GetError();
        Command command;
        command.Operation = Kind::Shared;
        command.Entity = entity;
        command.Type = binding.Type;
        command.Resource = binding.Object;
        return Append(std::move(command));
    }

    Result<void> CommandBuffer::RemoveShared(CommandEntity entity, TypeId type) {
        auto ready = CheckTarget(entity);
        if (!ready) return ready.GetError();
        Command command;
        command.Operation = Kind::RemoveShared;
        command.Entity = entity;
        command.Type = type;
        return Append(std::move(command));
    }

    Result<void> CommandBuffer::SetName(CommandEntity entity, std::string_view name) {
        auto ready = CheckTarget(entity);
        if (!ready) return ready.GetError();
        try {
            Command command;
            command.Operation = Kind::Name;
            command.Entity = entity;
            command.Name = name;
            return Append(std::move(command));
        }
        catch (const std::exception& exception) { return RecordFailure(exception.what()); }
        catch (...) { return RecordFailure("Recording the entity name failed"); }
    }

    Result<void> CommandBuffer::SetEnabled(CommandEntity entity, bool enabled) {
        auto ready = CheckTarget(entity);
        if (!ready) return ready.GetError();
        Command command;
        command.Operation = Kind::Enabled;
        command.Entity = entity;
        command.Present = enabled;
        return Append(std::move(command));
    }

    Result<void> CommandBuffer::SetComponentEnabled(CommandEntity entity, TypeId type, bool enabled) {
        auto ready = CheckTarget(entity);
        if (!ready) return ready.GetError();
        Command command;
        command.Operation = Kind::ComponentEnabled;
        command.Entity = entity;
        command.Type = type;
        command.Present = enabled;
        return Append(std::move(command));
    }

    Result<EntityId> CommandBuffer::Resolve(TemporaryEntity entity) const {
        if (m_Namespace == 0 || entity.Buffer != m_Namespace || entity.Index >= m_Resolved.size()) {
            return Error{ErrorCode::InvalidEntity, "ResolveCommands", "The temporary entity does not belong to this buffer"};
        }
        const auto resolved = m_Resolved[entity.Index];
        if (!resolved) {
            return Error{ErrorCode::InvalidState, "ResolveCommands", "The entity creation command has not succeeded"};
        }
        return resolved;
    }

    Result<EntityId> CommandBuffer::ResolveTarget(const CommandEntity& entity) const {
        if (const auto* direct = std::get_if<EntityId>(&entity)) return *direct;
        return Resolve(std::get<TemporaryEntity>(entity));
    }

    Result<void> CommandBuffer::Execute(Command& command, World& world) {
        if (command.Operation == Kind::Create) {
            auto created = world.CreateEmpty(command.Name, command.Uuid);
            if (!created) return created.GetError();
            m_Resolved[command.TemporaryIndex] = created.Value();
            return {};
        }
        if (command.Operation == Kind::Clear) return world.Clear();
        auto target = ResolveTarget(command.Entity);
        if (!target) return target.GetError();
        const EntityId entity = target.Value();
        switch (command.Operation) {
        case Kind::Clone: {
            auto cloned = world.Clone(entity, command.Name, command.Uuid);
            if (!cloned) return cloned.GetError();
            m_Resolved[command.TemporaryIndex] = cloned.Value();
            return {};
        }
        case Kind::Destroy: return world.Destroy(entity);
        case Kind::Set: return world.Set(entity, std::move(command.Value));
        case Kind::AddDefault: return world.AddDefault(entity, command.Type);
        case Kind::Remove: return world.Remove(entity, command.Type);
        case Kind::Tag: return world.SetTag(entity, command.Type, command.Present);
        case Kind::Shared: return world.SetShared(entity, SharedBinding{command.Type, command.Resource});
        case Kind::RemoveShared: return world.RemoveShared(entity, command.Type);
        case Kind::Name: return world.SetName(entity, command.Name);
        case Kind::Enabled: return world.SetEnabled(entity, command.Present);
        case Kind::ComponentEnabled: return world.SetComponentEnabled(entity, command.Type, command.Present);
        default: return Error{ErrorCode::InvalidState, "PlaybackCommands", "Unknown command operation"};
        }
    }

    Result<void> CommandBuffer::Playback(World& world) {
        auto ready = CheckRecording();
        if (!ready) return ready.GetError();
        if (&world.Context() != m_Context) {
            return Error{ErrorCode::InvalidArgument, "PlaybackCommands", "The World belongs to another Context"};
        }
        if (!m_Context->IsMainThread()) {
            return Error{ErrorCode::WrongThread, "PlaybackCommands", "Commands must be played on the Context owner thread"};
        }
        m_Consumed = true;
        for (size_t index = 0; index < m_Commands.size(); ++index) {
            try {
                auto result = Execute(m_Commands[index], world);
                if (!result) {
                    Error error = result.GetError();
                    error.Command = index + 1;
                    m_Commands.clear();
                    return error;
                }
                ++m_AppliedCount;
            }
            catch (const std::exception& exception) {
                Error error{ErrorCode::CommandFailed, "PlaybackCommands", exception.what(), 0, index + 1};
                m_Commands.clear();
                return error;
            }
            catch (...) {
                m_Commands.clear();
                return Error{ErrorCode::CommandFailed, "PlaybackCommands", "The command threw a non-standard exception", 0, index + 1};
            }
        }
        m_Commands.clear();
        return {};
    }
}
