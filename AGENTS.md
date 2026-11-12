# WholeUI — Agent Guide

WholeUI is a single Windhawk mod that gives Windows 11 one coherent, fluid interface, replacing
a stack of separate mods that each patch part of the screen. Read
[the constitution](.specify/memory/constitution.md) before doing anything: it defines the
principles, scope and workflow, and it overrides this guide.

## Status

New project. No spec is written yet. Features are listed with IDs in
[docs/product.md](docs/product.md) and the order of specs is in
[docs/roadmap.md](docs/roadmap.md).

## Starting a session

1. Read the constitution. Open `docs/product.md`, `docs/roadmap.md`,
   `docs/compatibility.md` and `docs/credits.md` when the task needs them.
2. Find the spec you are working on in `specs/<timestamp>-<slug>/` (the branch name
   `feat/<timestamp>-<slug>` points to it).
3. Read its `handoff.md` first, then `spec.md`, `plan.md` and `tasks.md` as needed. Do not read
   other specs unless the handoff says so.
4. At the end of the session, update `handoff.md` (template:
   `.specify/templates/handoff-template.md`).

## Keeping conversations short

Long conversations cost more and lose detail when they are summarized. Suggest that the
maintainer start a new conversation at natural break points: when a Spec Kit step finishes
(spec, plan, tasks, a group of implementation tasks), before switching to another spec, or
when the conversation has grown long. Update `handoff.md` first, so the new conversation can
continue from it alone.

## Rules that are easy to break

- **No code before the spec is approved** by the maintainer. Ask for explicit confirmation
  before writing any implementation code.
- **Never commit without the maintainer validating the change**, and **never push** unless
  asked. Show what changed and wait.
- Conventional Commits. **No `Co-Authored-By` or any AI attribution trailer.**
- Everything in the repository is in English. Talk to the maintainer in the language they
  use (currently Portuguese).
- A correction to an unmerged commit is folded into it (amend or fixup), not added as a new
  "fix" commit and never reverted.
- If a decision changes during implementation, update the spec before the code.
- Experiments stay on their own branch; their code is not merged unless a spec adopts it.
- **Never touch a window globally.** No `SetSysColors` or other system-wide changes; every
  change is scoped to windows the compatibility list allows (constitution I and II).
- **Never copy code from other mods.** Study them for ideas and record findings in prose in
  the spec's `research.md` (constitution VI).
- **No options by default.** The WinUI look, GPU acceleration and animations are not
  optional; a new setting needs a justification in the spec (constitution IV).
- Never edit the generated `.wh.cpp` by hand; change the sources and rebuild.

## Spec workflow (Spec Kit)

| Step | Skill | Output |
|---|---|---|
| Create the spec and its branch | `/speckit-specify` | `spec.md`, branch `feat/<timestamp>-<slug>` |
| Resolve open questions | `/speckit-clarify` | updated `spec.md` |
| Technical design | `/speckit-plan` | `plan.md` (+ research, data model, contracts) |
| Break into tasks | `/speckit-tasks` | `tasks.md` |
| Check consistency | `/speckit-analyze` | report |
| Implement (only after approval) | `/speckit-implement` | code |

Spec Kit auto-commit hooks are disabled on purpose; commits happen only after validation.
