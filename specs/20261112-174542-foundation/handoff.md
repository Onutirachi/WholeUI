# Handoff: Foundation

**Updated**: 2026-11-12 | **Spec**: [spec.md](spec.md) | **Branch**: `feat/20261112-174542-foundation`

## State

`spec.md` written and its three open questions answered by the maintainer; the quality
checklist passes. Waiting for the maintainer to review the spec. Nothing committed yet.

## Done in the last session

- Created the branch and the spec folder; wrote `spec.md` and `checklists/requirements.md`.
- Recorded the maintainer's answers in the spec's Clarifications section (FR-013a, FR-021,
  FR-021a, FR-021b).

## Decisions

- Context menus (F14) stay in roadmap item 6; their scope (classic Win32 menus only, or also
  menus of shell surfaces in `explorer.exe`) is decided in that spec.
- Shell surfaces in `explorer.exe` are always `native` and cannot be raised by overrides
  (FR-015).
- A child window never gets a higher level than its top-level window (FR-013); a dialog with
  hosted pages takes the lowest level among host and pages (FR-013a).
- Diagnostic mode: overlay next to the cursor plus a copy shortcut; identifies windows of any
  process, with "unknown" for unreadable fields.

## Open questions

- None in the spec. For the plan: how to know a property sheet's pages before it is shown
  (FR-013a), how to read creating module and template ID across processes (FR-021), Windhawk
  localization support (FR-009), Windhawk compiler version and flags for CI (FR-004).

## Next step

Maintainer reviews `spec.md`. Then optionally `/speckit-clarify`, then `/speckit-plan`.
