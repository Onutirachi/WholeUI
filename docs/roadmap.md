# WholeUI — Roadmap

Proposed order of specs. Each line becomes a spec folder when work starts. The order changes
only by maintainer decision.

1. Foundation: source layout, build script that generates the single `.wh.cpp`, CI with the
   Windhawk compiler, design tokens, compatibility list engine and diagnostic mode (F10, F11).
   **Implemented** (`specs/20261112-174542-foundation/`).
2. Explorer core: GPU file view, page transitions, Segoe UI Variable (F01, F02, F06).
3. GPU navigation pane matching the file view (F03).
4. Explorer backdrop with the three materials of S5 (F05). Starts with research on Mica,
   acrylic and the Luminosity approach.
5. Icons (F04) and Explorer details (F07).
6. Win32 appearance and dark mode by compatibility level (F12, F13, F14).
7. Small fixes: borders, folder picker, list views, Control Panel (F15-F18).
8. Shared backdrop and WinUI styling for validated legacy windows (F19).
9. Animated icons (F20), after everything above is finished and validated.
