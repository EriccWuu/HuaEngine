# Review Round 11

- Review method: independent agent
- Reviewer: McClintock
- Date: 2026-03-27
- Scope: T3.2 (`Operation Registry / Application Service Layer`)

## Findings

- No blocking issues.

## Follow-up Tightening Applied

- Removed raw domain-service headers from `HuaEngine.h` so the default host include surface no longer exposes `ProjectService / SceneService / AssetService / ScriptService / ValidationService`.
- Hid direct `ApplicationOperations` construction behind `Application` ownership and moved the public host chain to `Application::Start() -> GetOperations()`.
- Reworked `ApplicationOperationsSmoke` to validate the real host path instead of manually assembling `ApplicationServices`.
- Normalized aggregate validation results to the public operation id `validation.validate` so the operation layer does not leak the internal validation-domain id.

## Residual Risk

- Internal tests can still opt into raw services by explicitly including their headers, which is intentional for domain-level smoke coverage but should stay out of host code.
- `T3.2` centralizes the public operation layer, but formal CLI/headless dispatch and command parsing are still deferred to `T3.3`.

## Verdict

- No blocking issues.
- T3.2 can be considered closed.
