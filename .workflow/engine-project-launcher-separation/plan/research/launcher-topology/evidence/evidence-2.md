# E-2: Current startup ownership is still concentrated in EditorLayer

- Time: 2026-03-28
- Source type: repository code
- Confidence: A

## Observation

`Editor/src/EditorLayer.cpp` currently owns both:

- Project Hub entry rendering
- Workbench Shell rendering and panel orchestration

## Key Finding

Although Project Hub and Workbench Shell are now separated visually, they are still owned by the same runtime host and orchestration layer. This is sufficient for the current closure milestone, but it is not the clean launcher/workbench host split that the user now wants.

