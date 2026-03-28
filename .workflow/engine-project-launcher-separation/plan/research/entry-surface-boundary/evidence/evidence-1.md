# E-1: Current Project Hub shell is intentionally oversized with a fixed inner card

- Time: 2026-03-28
- Source type: repository code
- Confidence: A

## Observation

`EditorLayer::OnProjectHubShell()` creates a fullscreen shell and then centers a fixed-width/fixed-height child card.

## Key Finding

The user-visible "large window, small hub content" issue is not accidental. It comes directly from the current entry layout strategy.

