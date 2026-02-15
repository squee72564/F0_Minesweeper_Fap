# Minesweeper App Architecture Spec (Flipper Zero)

## 1. Purpose

Define and enforce a clean architecture where:

- Engine owns authoritative gameplay state and rules.
- Scene orchestrates action flow and side effects.
- View maps input and renders projection only.
- Solver work never blocks GUI responsiveness.

This document is normative for new work. If code disagrees with this spec, treat the code as migration-in-progress and move it toward this spec.

## 2. Current Direction and Status

### 2.1 Direction (Locked)

- Single source of truth for gameplay counters is `MineSweeperRuntime`.
- `MineSweeperBoard` is board truth + board metadata only.
- High-level engine APIs are `minesweeper_engine_*`.
- Win check is runtime-driven: all safe tiles cleared and all flags consumed against mines.

### 2.2 Status (as of current refactor)

Implemented in `engine/mine_sweeper_engine.*`:

- Runtime-owned progress counters (`tiles_left`, `flags_left`, `mines_left`).
- Board no longer tracks `revealed_count`.
- Engine actions:
  - `minesweeper_engine_new_game`
  - `minesweeper_engine_reveal`
  - `minesweeper_engine_chord`
  - `minesweeper_engine_toggle_flag`
  - `minesweeper_engine_move_cursor`
  - `minesweeper_engine_reveal_all_mines`
  - `minesweeper_engine_check_win_conditions`
  - `minesweeper_engine_apply_action`
- Engine state setup/restore APIs:
  - `minesweeper_engine_set_config`
  - `minesweeper_engine_set_runtime`
  - `minesweeper_engine_validate_state`
- Action semantics tightened:
  - reveal/chord return `NOOP` when no tile mutation occurs
  - `apply_action` blocks non-`NewGame` input once phase is `Won/Lost`

Still in progress:

- Add an explicit scene->view `apply_projection(...)` API (current view reads `MineSweeperState*` directly).
- Add coordinate bounds hardening for direct coordinate-taking engine calls used outside action dispatch.
- Move solver generation to non-blocking/chunked flow (deferred by product choice for now).

## 3. Architecture Layers

## 3.1 App Shell Layer

Owned by `MineSweeperApp` in `minesweeper.h`.

Responsibilities:

- Own scene manager, view dispatcher, notification/effects services.
- Own domain state (`MineSweeperState`, solver state).
- Wire module dependencies once at startup.

## 3.2 Scene Layer

Files: `scenes/*.c`

Responsibilities:

- Handle lifecycle (`on_enter`, `on_event`, `on_exit`).
- Receive high-level user actions from view.
- Call engine APIs and interpret `MineSweeperResult`.
- Trigger side effects (haptic/LED/speaker) based on engine result.
- Push fresh engine projection to the view model.

Non-responsibilities:

- No direct low-level cell mutation.
- No long solver loops.

## 3.3 View Layer

Files: `views/*`

Responsibilities:

- Draw from a projection snapshot.
- Map `InputEvent` into higher-level game actions.
- Keep only UI-local state (viewport offsets, UI flags, cached render strings).

Non-responsibilities:

- No authoritative board-rule decisions.
- No win/loss logic.
- No board generation or solving loops.

Note about firmware callback signatures:

- `view_set_input_callback` must use Flipper's raw input callback signature.
- The view should internally map raw input to action payloads and forward them via an app-level callback.

## 3.4 Engine Layer

Primary module: `engine/mine_sweeper_engine.*`.

Responsibilities:

- Own gameplay rules and state transitions.
- Execute reveal/chord/flag/move actions.
- Own win/loss phase transitions.
- Maintain runtime counters and invariants.

Canonical state container:

```c
typedef struct {
    MineSweeperBoard board;
    MineSweeperConfig config;
    MineSweeperRuntime rt;
} MineSweeperState;
```

## 4. Authoritative Data Contract

## 4.1 Board State (`MineSweeperBoard`)

Allowed responsibilities:

- Board dimensions.
- `mine_count`.
- Packed cell truth (`mine/revealed/flag/neighbor_count`).

Not allowed:

- Runtime progression counters (`revealed_count`, `tiles_left`, etc.).

## 4.2 Runtime State (`MineSweeperRuntime`)

Authoritative gameplay runtime fields:

- `cursor_row`, `cursor_col`
- `tiles_left`
- `flags_left`
- `mines_left`
- `start_tick`
- `phase`

Counter semantics:

- `tiles_left`: unrevealed non-mine tile count remaining.
- `flags_left`: number of flags that can still be placed.
- `mines_left`: mines remaining to be correctly flagged (runtime progress counter, not static mine total).

## 4.3 Win/Loss Contract

Loss:

- Triggered by revealing a mine (direct reveal or chord reveal path).
- Engine sets `phase = MineSweeperPhaseLost`.
- Engine reveals all mines (`minesweeper_engine_reveal_all_mines`).

Win:

- Triggered when both hold:
  - `rt.tiles_left == 0`
  - `rt.flags_left == rt.mines_left`
- Engine sets `phase = MineSweeperPhaseWon`.

## 4.4 Runtime Invariants

These invariants should hold after every engine action:

- `0 <= rt.tiles_left <= (board.width * board.height - board.mine_count)`
- `0 <= rt.flags_left <= board.mine_count`
- `0 <= rt.mines_left <= board.mine_count`
- If `rt.phase == MineSweeperPhaseWon`, then `rt.tiles_left == 0`.
- If `rt.phase == MineSweeperPhaseLost`, mine reveal pass has executed.

## 5. Engine API Contract

Current high-level API direction:

- `void minesweeper_engine_new_game(MineSweeperState* game_state)`
- `MineSweeperResult minesweeper_engine_reveal(MineSweeperState* game_state, uint16_t x, uint16_t y)`
- `MineSweeperResult minesweeper_engine_chord(MineSweeperState* game_state, uint16_t x, uint16_t y)`
- `MineSweeperResult minesweeper_engine_toggle_flag(MineSweeperState* game_state, uint16_t x, uint16_t y)`
- `MineSweeperResult minesweeper_engine_move_cursor(MineSweeperState* game_state, int8_t dx, int8_t dy)`
- `MineSweeperResult minesweeper_engine_reveal_all_mines(MineSweeperState* game_state)`
- `MineSweeperResult minesweeper_engine_check_win_conditions(MineSweeperState* game_state)`
- `MineSweeperResult minesweeper_engine_apply_action(MineSweeperState* game_state, MineSweeperAction action)`
- `MineSweeperResult minesweeper_engine_set_config(MineSweeperState* game_state, const MineSweeperConfig* config)`
- `MineSweeperResult minesweeper_engine_set_runtime(MineSweeperState* game_state, const MineSweeperRuntime* runtime)`
- `MineSweeperResult minesweeper_engine_validate_state(const MineSweeperState* game_state)`

Action dispatch contract:

- `MineSweeperAction` is the scene/view payload for user intent.
- Action type covers at least: `Move`, `Reveal`, `Flag`, `Chord`, `NewGame`.
- `Move` uses `dx`/`dy`; other actions operate at runtime cursor unless explicit coordinates are later added.
- `minesweeper_engine_apply_action` routes to specialized engine functions and returns final result code.
- `minesweeper_engine_set_config` validates config and syncs board dimensions.
- `minesweeper_engine_set_runtime` validates runtime against current board metadata before applying.
- `minesweeper_engine_validate_state` validates config/board/runtime invariants for restore/load paths.
- Scene should call this as the default gameplay entry point.

Result semantics:

- `NOOP`: valid action with no state change.
- `CHANGED`: state changed without terminal transition.
- `WIN`: win transition occurred.
- `LOSE`: loss transition occurred.
- `INVALID`: invalid parameters/state.

Low-level board functions are internal mutation primitives and must not own runtime counters.

## 6. Input and Action Flow

Target flow:

1. Firmware sends `InputEvent` into view input callback.
2. View maps input to high-level `MineSweeperEvent`.
3. View forwards event via scene-registered callback.
4. Scene maps event to `MineSweeperAction` and calls engine action dispatch.
5. Engine returns result + mutated state.
6. Scene runs effects and transitions.
7. Scene projects engine snapshot into view model.
8. View redraws.

## 7. Scene/View Contracts

## 7.1 Scene -> View

At scene enter:

- Set context.
- Set action callback.
- Push initial projection snapshot.

At scene exit:

- Unset callback/context.
- Clear temporary scene-owned resources.

## 7.2 View -> Scene

View should expose explicit APIs:

- `set_context(...)`
- `set_input_callback(...)` (high-level game events)
- `apply_projection(...)` (recommended next step; currently deferred)

View should return consumed/not-consumed from raw input callback, but rule execution belongs to scene+engine.

## 8. Deprecated and Outdated Patterns

The following are now considered technical debt and should be removed during migration:

- Authoritative board and counters in `MineSweeperGameScreenModel`.
- View-owned win/loss and reveal/chord/flag rules.
- View-owned board generation/solver loops.
- Duplicate counters with parity assumptions across board/runtime.

## 9. File Mapping

- Engine: `engine/mine_sweeper_engine.h`, `engine/mine_sweeper_engine.c`
- Solver: `engine/mine_sweeper_solver.h`, `engine/mine_sweeper_solver.c`
- Gameplay scene orchestrator: `scenes/game_scene.c`
- Gameplay view: `views/minesweeper_game_screen2.h`, `views/minesweeper_game_screen2.c`

## 10. Migration Plan (Updated)

## Stage 1 (Completed)

- Introduced `MineSweeperState` split (`board/config/rt`).
- Introduced `minesweeper_engine_*` naming for high-level APIs.
- Removed board-side `revealed_count` runtime duplication.
- Added runtime-based win check API.
- Added engine-level toggle flag, cursor move, and reveal-all-mines actions.
- Added `MineSweeperAction` + `minesweeper_engine_apply_action(...)` dispatch path.
- Added config/runtime/state validation APIs for restore flows.
- Aligned reveal/chord return behavior to true mutation semantics (`NOOP` vs `CHANGED`).

## Stage 2 (Completed)

- Added game screen setter for scene-owned callback.
- Moved input mapping in view to high-level event emission.
- Moved gameplay action handling into `scenes/game_scene.c` through engine action dispatch.
- Removed legacy gameplay scene/view modules from active build path.
- Kept gameplay rules in engine and side effects in scene.

## Stage 3

- Add explicit `apply_projection(...)` scene->view API (currently view reads state pointer directly).
- Add targeted hardening for direct coordinate engine entry points.
- Verify all side effects are scene-triggered from engine result codes.

## Stage 4

- Integrate non-blocking solver service (chunked, event-driven) when required by UX/perf goals.
- Keep solver off heavy loops in view callbacks.

## 11. Validation Matrix

Run after each migration stage:

1. Build: `uvx ufbt`
2. Input smoke:
   - movement, reveal, flag, chord
   - no duplicate handling paths
3. Terminal states:
   - mine hit reveals all mines + loss phase
   - win only when runtime win conditions are met
4. Navigation:
   - scene transitions and back behavior remain stable
5. Stability:
   - repeated game reset/start/exit cycles

## 12. Engine Dispatch Guideline

`minesweeper_engine_apply_action_ex(...)` is the preferred engine entry point from scene callbacks when move outcome metadata is needed for feedback.

`minesweeper_engine_apply_action(...)` remains a compatibility wrapper returning the coarse result code only.

Guideline:

- Keep specialized engine actions (`reveal`, `flag`, `chord`, `move_cursor`, `new_game`) for focused testing and reuse.
- Keep `apply_action_ex` as a thin, deterministic dispatcher over those specialized functions.
- Keep side effects and scene transitions outside engine.

Benefits:

- Single scene call path for gameplay input.
- Consistent result handling (`NOOP/CHANGED/WIN/LOSE/INVALID`).
- Cleaner scene code and easier input contract evolution.

## 13. Immediate Next Steps

1. Add an explicit scene->view `apply_projection(...)` path for cleaner snapshot boundaries.
2. Add/confirm coordinate bounds checks for direct coordinate-taking engine calls used outside action dispatch.
3. If needed, migrate solvable-board generation to non-blocking background/chunked flow.
