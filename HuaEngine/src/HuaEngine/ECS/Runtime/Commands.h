#pragma once

#include "HuaEngine/ECS/Runtime/World.h"

#include <string>
#include <variant>
#include <vector>

namespace HE::Ecs {
    struct TemporaryEntity {
        uint64_t Buffer = 0;
        uint32_t Index = 0;
        explicit operator bool() const noexcept { return Buffer != 0; }
        auto operator<=>(const TemporaryEntity&) const = default;
    };

    using CommandEntity = std::variant<EntityId, TemporaryEntity>;

    // A buffer is single-owner and its Context must outlive it and its values.
    class CommandBuffer final {
    public:
        explicit CommandBuffer(EcsContext& context);
        ~CommandBuffer() = default;
        CommandBuffer(const CommandBuffer&) = delete;
        CommandBuffer& operator=(const CommandBuffer&) = delete;
        CommandBuffer(CommandBuffer&& other) noexcept;
        CommandBuffer& operator=(CommandBuffer&& other) noexcept;

        [[nodiscard]] Result<TemporaryEntity> CreateEmpty(std::string_view name = "Entity", EntityUuid uuid = {});
        [[nodiscard]] Result<TemporaryEntity> Clone(CommandEntity source, std::string_view name = {}, EntityUuid uuid = {});
        [[nodiscard]] Result<void> Destroy(CommandEntity entity);
        [[nodiscard]] Result<void> ClearWorld();
        [[nodiscard]] Result<void> Set(CommandEntity entity, OwnedValue&& value);
        [[nodiscard]] Result<void> AddDefault(CommandEntity entity, TypeId type);
        [[nodiscard]] Result<void> Remove(CommandEntity entity, TypeId type);
        [[nodiscard]] Result<void> SetTag(CommandEntity entity, TypeId type, bool present = true);
        [[nodiscard]] Result<void> SetShared(CommandEntity entity, SharedBinding binding);
        [[nodiscard]] Result<void> RemoveShared(CommandEntity entity, TypeId type);
        [[nodiscard]] Result<void> SetName(CommandEntity entity, std::string_view name);
        [[nodiscard]] Result<void> SetEnabled(CommandEntity entity, bool enabled);
        [[nodiscard]] Result<void> SetComponentEnabled(CommandEntity entity, TypeId type, bool enabled);

        // Preflight failures keep the buffer intact. Execution consumes it once.
        // Successful commands remain committed; Error.Command is one-based.
        [[nodiscard]] Result<void> Playback(World& world);
        // A mapping remains available after failure, even if a later command destroyed it.
        [[nodiscard]] Result<EntityId> Resolve(TemporaryEntity entity) const;
        [[nodiscard]] size_t AppliedCount() const noexcept { return m_AppliedCount; }
        [[nodiscard]] size_t CommandCount() const noexcept { return m_RecordedCount; }
        [[nodiscard]] bool Consumed() const noexcept { return m_Consumed; }

    private:
        enum class Kind { Create, Clone, Destroy, Clear, Set, AddDefault, Remove, Tag, Shared, RemoveShared, Name, Enabled, ComponentEnabled };
        struct Command {
            Kind Operation = Kind::Destroy;
            CommandEntity Entity;
            OwnedValue Value;
            std::string Name;
            EntityUuid Uuid;
            TypeId Type = InvalidTypeId;
            ResourceHandle Resource;
            uint32_t TemporaryIndex = 0;
            bool Present = true;
        };

        [[nodiscard]] Result<void> CheckRecording() const;
        [[nodiscard]] Result<void> CheckTarget(const CommandEntity& entity) const;
        [[nodiscard]] Result<void> Append(Command&& command);
        [[nodiscard]] Result<TemporaryEntity> AppendCreation(Command&& command);
        [[nodiscard]] Result<EntityId> ResolveTarget(const CommandEntity& entity) const;
        [[nodiscard]] Result<void> Execute(Command& command, World& world);

        EcsContext* m_Context = nullptr;
        uint64_t m_Namespace = 0;
        bool m_Consumed = false;
        size_t m_RecordedCount = 0;
        size_t m_AppliedCount = 0;
        std::vector<Command> m_Commands;
        std::vector<EntityId> m_Resolved;
    };
}
