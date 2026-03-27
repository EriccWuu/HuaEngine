# Review Round 10

- Review method: independent agent
- Reviewer: Faraday
- Date: 2026-03-27
- Scope: T3.1 (`ApplicationServices / service composition root`)

## Findings

- No blocking issues.

## Follow-up Tightening Applied

- Centralized the five domain services into `ApplicationServices` so host startup no longer needs to assemble services ad hoc.
- Moved `ValidationService` to injected domain-service dependencies so aggregate validation consumes the same service instances exposed by the composition root.
- Added `ApplicationServicesSmoke` plus updated validation smoke coverage to prove the shared composition root can drive project, asset, script, and validation flows end to end.

## Residual Risk

- `Application` still publicly exposes raw domain services through the composition root; the host-facing operation boundary is not in place until `T3.2`.
- Service registration is now centralized, but hosts can still choose to orchestrate those services directly until the unified operation layer is introduced.

## Verdict

- No blocking issues.
- T3.1 can be considered closed.
