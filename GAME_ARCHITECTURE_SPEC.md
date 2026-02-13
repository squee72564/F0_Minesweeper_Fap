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

- Single source of truth for gameplay counters is `MineGameRuntime`.
- `MineSweeperGameBoard` is board truth + board metadata only.
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

Still in progress:

- View is still performing substantial gameplay mutations directly.
- Scene is not yet the primary gameplay orchestrator.
- Game screen view callback contract still forwards raw `InputEvent` only.

## 3. Architecture Layers

## 3.1 App Shell Layer

Owned by `MineSweeperApp` in `minesweeper.h`.

Responsibilities:

- Own scene manager, view dispatcher, notification/effects services.
- Own domain state (`MineSweeperGameState`, solver state).
- Wire module dependencies once at startup.

## 3.2 Scene Layer

Files: `scenes/*.c`

Responsibilities:

- Handle lifecycle (`on_enter`, `on_event`, `on_exit`).
- Receive high-level user actions from view.
- Call engine APIs and interpret `MineSweeperGameResult`.
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
    MineSweeperGameBoard board;
    MineGameConfig config;
    MineGameRuntime rt;
} MineSweeperGameState;
```

## 4. Authoritative Data Contract

## 4.1 Board State (`MineSweeperGameBoard`)

Allowed responsibilities:

- Board dimensions.
- `mine_count`.
- Packed cell truth (`mine/revealed/flag/neighbor_count`).

Not allowed:

- Runtime progression counters (`revealed_count`, `tiles_left`, etc.).

## 4.2 Runtime State (`MineGameRuntime`)

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
- Engine sets `phase = MineGamePhaseLost`.
- Engine reveals all mines (`minesweeper_engine_reveal_all_mines`).

Win:

- Triggered when both hold:
  - `rt.tiles_left == 0`
  - `rt.flags_left == rt.mines_left`
- Engine sets `phase = MineGamePhaseWon`.

## 4.4 Runtime Invariants

These invariants should hold after every engine action:

- `0 <= rt.tiles_left <= (board.width * board.height - board.mine_count)`
- `0 <= rt.flags_left <= board.mine_count`
- `0 <= rt.mines_left <= board.mine_count`
- If `rt.phase == MineGamePhaseWon`, then `rt.tiles_left == 0`.
- If `rt.phase == MineGamePhaseLost`, mine reveal pass has executed.

## 5. Engine API Contract

Current high-level API direction:

- `void minesweeper_engine_new_game(MineSweeperGameState* game_state)`
- `MineSweeperGameResult minesweeper_engine_reveal(MineSweeperGameState* game_state, uint16_t x, uint16_t y)`
- `MineSweeperGameResult minesweeper_engine_chord(MineSweeperGameState* game_state, uint16_t x, uint16_t y)`
- `MineSweeperGameResult minesweeper_engine_toggle_flag(MineSweeperGameState* game_state, uint16_t x, uint16_t y)`
- `MineSweeperGameResult minesweeper_engine_move_cursor(MineSweeperGameState* game_state, int8_t dx, int8_t dy)`
- `MineSweeperGameResult minesweeper_engine_reveal_all_mines(MineSweeperGameState* game_state)`
- `MineSweeperGameResult minesweeper_engine_check_win_conditions(MineSweeperGameState* game_state)`
- `MineSweeperGameResult minesweeper_engine_apply_action(MineSweeperGameState* game_state, MineGameAction action)`

Action dispatch contract:

- `MineGameAction` is the scene/view payload for user intent.
- Action type covers at least: `Move`, `Reveal`, `Flag`, `Chord`, `NewGame`.
- `Move` uses `dx`/`dy`; other actions operate at runtime cursor unless explicit coordinates are later added.
- `minesweeper_engine_apply_action` routes to specialized engine functions and returns final result code.
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
2. View maps input to `MineGameAction` payload.
3. View forwards action via scene-registered callback.
4. Scene calls `minesweeper_engine_apply_action(...)` (default path).
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
- `set_action_callback(...)`
- `apply_projection(...)`

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
- Gameplay scene orchestrator: `scenes/game_screen_scene.c`
- Gameplay view: `views/minesweeper_game_screen.h`, `views/minesweeper_game_screen.c`

## 10. Migration Plan (Updated)

## Stage 1 (Completed)

- Introduced `MineSweeperGameState` split (`board/config/rt`).
- Introduced `minesweeper_engine_*` naming for high-level APIs.
- Removed board-side `revealed_count` runtime duplication.
- Added runtime-based win check API.
- Added engine-level toggle flag cursor move, and reveal-all-mines actions.

## Stage 2 (Next)

- Add game screen setter for scene-owned action callback (parallel to start screen pattern).
- Move input mapping in view from direct mutation to action emission.
- Add `MineGameAction` payload contract shared by view/scene/engine.
- Add `minesweeper_engine_apply_action(...)` and route scene gameplay through it.
- Move gameplay action handling into `scenes/game_screen_scene.c` via `apply_action`.
- Keep view as projection-only for authoritative gameplay data.

## Stage 3

- Remove remaining direct board mutation helpers from view gameplay paths.
- Ensure scene is sole path to call engine gameplay actions.
- Verify all side effects are scene-triggered from engine result codes.

## Stage 4

- Integrate non-blocking solver service (chunked, event-driven).
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

`minesweeper_engine_apply_action(...)` is the preferred engine entry point from scene callbacks.

Guideline:

- Keep specialized engine actions (`reveal`, `flag`, `chord`, `move_cursor`, `new_game`) for focused testing and reuse.
- Keep `apply_action` as a thin, deterministic dispatcher over those specialized functions.
- Keep side effects and scene transitions outside engine.

Benefits:

- Single scene call path for gameplay input.
- Consistent result handling (`NOOP/CHANGED/WIN/LOSE/INVALID`).
- Cleaner scene code and easier input contract evolution.
