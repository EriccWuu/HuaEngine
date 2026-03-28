# Research: entry-surface-boundary

## Scope

Decide what should remain inside Editor startup UI after introducing a dedicated launcher host.

## Gather

- [E-1] Current standalone hub shell is fullscreen with a fixed inner card, which causes the perceived oversized entry window.
- [E-2] The actual workbench panel model is already coherent after project activation.

## Analyze

There are two boundary choices:

| Choice | Editor keeps | Launcher owns | Assessment |
|--------|--------------|---------------|------------|
| A | full embedded Project Hub fallback UI | optional separate launcher | boundary stays blurry |
| B | minimal fallback handoff/error surface only | full create/open/resume project UI | clearer and closer to modern engines |

## Compare

Choice B better matches the product direction.

- The launcher can own spacious create/open/recent-project UI without polluting Editor runtime.
- The current layout issue can be solved at the product boundary, not just by resizing the card. [E-1]
- The existing workbench can stay focused on project editing. [E-2]

## Recommend

Recommend **Choice B**:

- `ProjectHub.exe` owns the real entry experience and recent-project surface.
- `Editor.exe` keeps only a minimal no-project fallback surface, mainly to recover when launched incorrectly or when launcher handoff fails.
- The current oversized embedded launcher card should not be refined into a long-term Editor startup mode.

## Sources

- [E-1](evidence/evidence-1.md)
- [E-2](evidence/evidence-2.md)

