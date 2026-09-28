#pragma once

#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/ECS/Runtime/Result.h"

namespace HE {
    inline ResultEnvelope EcsFailureEnvelope(std::string operation, std::string target, const Ecs::Error& error) {
        auto result = ResultEnvelope::Failure(std::move(operation), std::move(target), error.Message);
        result.AddDetail({DiagnosticSeverity::Error, "ecs.operation.failed", error.Message, error.Operation});
        result.SetPayloadValue("ecs_error_code", std::to_string(static_cast<int>(error.Code)));
        if (error.Task != 0) result.SetPayloadValue("ecs_task", std::to_string(error.Task));
        if (error.Command != 0) result.SetPayloadValue("ecs_command", std::to_string(error.Command));
        return result;
    }
}
