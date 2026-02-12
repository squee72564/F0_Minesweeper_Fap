# Minesweeper App Architecture Spec (Flipper Zero)

## 1. Purpose

Define a clean, modular architecture for the Minesweeper app that:

- Separates scene flow from view rendering.
- Keeps game rules and board state in a dedicated logic layer.
- Maintains responsive input/render behavior on all board sizes.
- Supports a robust solver that does not block the GUI thread.
- Leaves room for a future multithreaded solver implementation.

This is a high-level technical spec for structure and control flow, not a line-by-line implementation plan.

## 2. Architectural Principles

- Single source of truth: game state is owned in one place (logic/engine state), not duplicated across scene/view as authoritative data.
- Thin scenes: scenes orchestrate navigation and lifecycle, but do not implement low-level board rules.
- Thin views: views draw and capture input, but do not run heavy game logic loops.
- Non-blocking GUI thread: no long loops in draw/input/enter callbacks.
- Explicit data contracts: scene-to-view update APIs define what is projected and when.

## 3. Layered Design

## 3.1 App Shell Layer

Owned by `MineSweeperApp` in `minesweeper.h`.

Responsibilities:

- Hold global app services: `SceneManager`, `ViewDispatcher`, notification service.
- Own scene modules and view modules.
- Own shared game domain objects (engine + solver state).
- Coordinate startup/shutdown and dependency wiring.

## 3.2 Scene Layer

Files: `scenes/*.c`

Responsibilities:

- Handle lifecycle: `on_enter`, `on_event`, `on_exit`.
- Set view context and callbacks during scene enter.
- Convert high-level view actions into engine operations.
- Route custom events and scene transitions.
- Trigger view model refresh after state changes.

Non-responsibilities:

- No heavy board solving loops.
- No direct rendering logic.

## 3.3 View Layer

Files: `views/start_screen.*`, `views/minesweeper_game_screen.*`

Responsibilities:

- Draw from a view model snapshot.
- Receive raw `InputEvent` and translate into action callbacks.
- Own only UI-local state (cursor animation flags, viewport offsets, text/icon state, etc.).

Non-responsibilities:

- No ownership of authoritative board rules state.
- No long-running computation.

## 3.4 Game Logic Layer (Engine)

Recommended new module, for example `game/mine_sweeper_engine.*`.

Responsibilities:

- Own board truth, tile states, rule validation, win/loss conditions.
- Execute player actions: move, reveal, chord, flag.
- Maintain deterministic state transitions.
- Expose read-only snapshots for rendering and derived status.

API style:

- Stateless-like operations over owned state object or instance methods over engine struct.
- Clear result codes (`NoOp`, `Changed`, `Win`, `Lose`, `Invalid`).

## 3.5 Solver Layer

Recommended new module, for example `game/mine_sweeper_solver.*`.

Responsibilities:

- Generate or verify solvable boards.
- Run incrementally with bounded work per step.
- Track attempts and stop conditions (iteration cap and timeout).

Non-responsibilities:

- Do not touch GUI or view APIs directly.

## 4. Data Ownership and Access

## 4.1 Authoritative Data

Owned by app-level domain state:

- `MineSweeperGameEngine` (board, revealed/flagged state, mines, cursor position, counters).
- `MineSweeperSolverState` (attempt count, progress status, pending job data).
- Settings (`MineSweeperAppSettings` and active settings fields).

## 4.2 View Model Data

Owned by view model structs:

- Render projection of board and status.
- Viewport/window offsets and visual-only state.
- Optional cached strings.

Rule:

- View model is a projection, not source of truth.
- Scene updates view model from engine snapshot after each meaningful change.

## 4.3 Context Wiring

Use context pointers consistently:

- Scene callbacks receive `MineSweeperApp*`.
- View callbacks receive view instance context set by scene/app.
- Input callback chain:
  - View receives raw `InputEvent`.
  - View maps input to `GameAction`.
  - View callback sends action to scene/app context.

## 5. Input and Update Control Flow

## 5.1 High-Level Flow

1. User input enters active view via `view_set_input_callback`.
2. View maps raw key/type to action (`MoveUp`, `Reveal`, `Flag`, `Chord`, `OpenMenu`).
3. View callback invokes scene-provided action handler with app context.
4. Scene applies action to engine.
5. Engine returns result and updated state.
6. Scene triggers side effects (LED/haptic/sound) based on result.
7. Scene projects engine snapshot into view model and requests redraw.
8. Scene may emit transition events (`win`, `lose`, `menu`, etc.).

## 5.2 Event Routing

- Use `scene_manager_handle_custom_event` for scene transitions and async solver progress events.
- Keep event IDs explicit and scoped per scene/module.
- Ensure all scene events are either consumed or intentionally ignored.

## 6. Rendering Model

- Draw callback reads view model only.
- Draw callback must avoid business logic and expensive loops.
- Any expensive formatting or board transforms should be precomputed during projection/update phase.

## 7. Robust Solver Design (Single-Thread, Non-Blocking)

## 7.1 Problem

Large hard boards may require many attempts; a continuous solver loop inside a view callback can freeze viewport/input.

## 7.2 Required Behavior

- Solver must never monopolize the GUI thread.
- Solver work must be chunked into bounded units.
- Solver must terminate with a deterministic fallback.

## 7.3 Solver State Machine

Suggested states:

- `Idle`
- `Running`
- `Succeeded`
- `FailedMaxAttempts`
- `FailedTimeout`
- `Cancelled`

## 7.4 Chunked Execution Model

- Start solve request from scene, not view.
- Process `N` iterations per tick (configurable budget).
- After each chunk, yield and schedule next tick with timer/custom event.
- Update loading/progress UI between chunks.

Suggested limits:

- `max_attempts` hard cap.
- `max_time_ms` timeout.
- `iterations_per_tick` budget tuned for responsiveness.

## 7.5 Completion and Fallback

On completion:

- `Succeeded`: commit board to engine, refresh view, continue gameplay.
- `FailedMaxAttempts` or `FailedTimeout`: use fallback board policy (best-effort or unsolved board), log reason, continue without freeze.

## 8. Scene and View Contracts

## 8.1 Scene -> View Contract

Scene sets at enter:

- Context pointer
- Input callback
- Secondary draw callback (if needed)
- Initial render projection

Scene resets at exit:

- Input callback and context pointers
- Any optional secondary callbacks
- Any temporary view resources

## 8.2 View -> Scene Contract

View exposes:

- `set_context(...)`
- `set_input_callback(...)`
- `set_model_projection(...)` or equivalent focused setters

View callback returns consumed/not consumed without direct scene transition logic.

## 9. Reliability and Safety Requirements

- Assert all callback contexts and required pointers.
- Ensure alloc/reset/free lifecycle consistency for view resources.
- Keep scene enter/exit idempotent where practical.
- Maintain strict cleanup of timers/solver jobs on scene exit/app shutdown.

## 10. Observability and Diagnostics

- Add concise logs around solver state transitions and stop reasons.
- Log attempt count and elapsed time for solver completion/failure.
- Keep logs bounded and avoid per-iteration noise in release builds.

## 11. Migration Strategy (From Current Structure)

1. Introduce engine module while preserving existing behavior.
2. Move board-rule functions from view into engine gradually.
3. Change view to consume projected engine snapshots only.
4. Introduce chunked solver service and route events through scene manager.
5. Remove remaining heavy logic from view callbacks.

Perform this incrementally in small commits to preserve bisectability.

## 12. Optional Future Section: Multithreaded Solver

This section is optional and can be implemented later for faster large-board solving.

## 12.1 Goals

- Improve solve throughput for large/high-difficulty boards.
- Keep GUI thread fully responsive.

## 12.2 Constraints

- Worker thread must not call GUI/view APIs directly.
- Shared state must use message queues, events, or guarded handoff data.
- Cancellation must be explicit on scene exit/app shutdown.

## 12.3 Suggested Architecture

- Scene starts solver job by submitting request payload to worker queue.
- Worker computes and posts progress/result messages back to scene queue.
- Scene consumes messages, updates solver state, and refreshes view.

Message types:

- `SolverProgress`
- `SolverSuccess`
- `SolverFailure`
- `SolverCancelledAck`

## 12.4 Safety Rules

- One active solver job per game session.
- Job IDs required to ignore stale results.
- Hard timeout and attempt cap still required, even off-thread.
- Ensure thread join/shutdown on app stop.

## 12.5 Rollout Plan

1. Keep single-thread chunked solver as baseline.
2. Add worker abstraction behind same solver interface.
3. Feature-flag multithreaded mode for testing.
4. Validate stability under repeated start/exit/relaunch cycles.

## 13. Mapping to Current Files

This section maps the target architecture to the current repository layout.

## 13.1 App Shell and Wiring

- `minesweeper.h`
  - Keep `MineSweeperApp` as the shared root context.
  - Add engine and solver handles/state fields here.
- `minesweeper.c`
  - Allocate/free engine and solver modules.
  - Keep scene manager and view dispatcher setup here.
  - Wire callbacks and shared context once at startup.

## 13.2 Scene Layer

- `scenes/game_screen_scene.c`
  - Be the primary orchestrator for gameplay.
  - On enter: bind game view callbacks/context and initialize projection from engine snapshot.
  - On event: process custom solver-progress/result events and state transitions.
  - On exit: cancel/cleanup solver jobs and unbind view callback/context.
- `scenes/start_screen_scene.c`, `scenes/menu_scene.c`, `scenes/settings_scene.c`, `scenes/confirmation_scene.c`, `scenes/info_scene.c`
  - Keep flow/navigation concerns only.
  - Avoid embedding board-rule logic.

## 13.3 View Layer

- `views/minesweeper_game_screen.c`
  - Retain rendering and input mapping responsibilities.
  - Remove authoritative board-rule ownership over time.
  - Expose action callback API (`on_action`) and projection update API.
- `views/minesweeper_game_screen.h`
  - Add explicit contracts for:
    - setting action callback + context,
    - applying render projection/snapshot.
- `views/minesweeper_game_screen_i.h`
  - Keep private helper/model definitions only.
- `views/start_screen.c`, `views/start_screen.h`
  - Keep as view-only module (already aligned by Stage 12 cleanup).

## 13.4 Logic and Solver Modules (New)

Recommended new files:

- `game/mine_sweeper_engine.h`
- `game/mine_sweeper_engine.c`
- `game/mine_sweeper_solver.h`
- `game/mine_sweeper_solver.c`

If introducing a new `game/` folder is inconvenient, equivalent files can be placed under `helpers/` as an intermediate step.

## 13.5 Side-Effect Helpers

- `helpers/mine_sweeper_haptic.*`
- `helpers/mine_sweeper_led.*`
- `helpers/mine_sweeper_speaker.*`

Use these as effect backends called by scene logic based on engine result codes, not as rule decision makers.

## 14. Staged Refactor Checklist

This checklist is intentionally incremental and low-risk.

## Stage A: Define Engine Interface

Goal:

- Introduce engine API without changing gameplay behavior.

Tasks:

1. Create `mine_sweeper_engine.h/.c` with state struct and minimal action/result enums.
2. Add translation helpers to import/export current board state format.
3. Add a thin adapter path so existing view logic can call engine functions while behavior remains equivalent.

Exit criteria:

- Build passes with no behavior change.

## Stage B: Move Rule Logic Out of View

Goal:

- Shift board mutation and win/loss logic from `views/minesweeper_game_screen.c` into engine.

Tasks:

1. Move tile reveal/flag/chord and win/loss evaluation to engine APIs.
2. Keep view responsible only for action mapping and drawing.
3. Update scene to call engine and then push projection to view.

Exit criteria:

- Gameplay still matches current behavior.
- View callbacks contain no heavy board-rule loops.

## Stage C: Introduce Non-Blocking Solver Service

Goal:

- Replace any long-running solve loop with chunked incremental processing.

Tasks:

1. Add solver state struct (`Idle/Running/Succeeded/Failed/Cancelled`).
2. Add `solver_start`, `solver_step`, `solver_cancel`.
3. Use timer/custom events to run `solver_step` with bounded iteration budget.
4. Enforce `max_attempts` and `max_time_ms` with deterministic fallback.

Exit criteria:

- Large-board solve no longer freezes viewport/input.
- Solver always terminates with success or explicit fallback.

## Stage D: Scene-Orchestrated Projection Updates

Goal:

- Make scene the explicit coordinator of action -> engine -> projection -> redraw.

Tasks:

1. Add action callback contract from view to scene.
2. Add snapshot/projection update API from scene to view.
3. Centralize effect triggering in scene based on engine result codes.

Exit criteria:

- Clear unidirectional flow and no hidden state mutation across layers.

## Stage E: Hardening and Cleanup

Goal:

- Stabilize lifecycle and cleanup behavior.

Tasks:

1. Add assertions and null-guards for all context/callback boundaries.
2. Verify scene enter/exit idempotence and solver cancellation on exit.
3. Remove obsolete helper functions and dead state fields.

Exit criteria:

- Clean alloc/reset/free behavior across repeated relaunch cycles.

## Stage F (Optional): Multithreaded Solver

Goal:

- Improve solve throughput while preserving GUI responsiveness.

Tasks:

1. Add worker thread behind same solver interface.
2. Use message passing for progress/result; no GUI access from worker.
3. Add job IDs and cancellation on scene exit/app shutdown.

Exit criteria:

- No race-induced crashes across repeated start/stop cycles.
- Performance gain measurable on large hard boards.

## 15. Validation Matrix

Run after each stage:

1. Build: `uvx ufbt`
2. Smoke test:
   - app launch -> start screen -> game
   - movement/flag/reveal/chord paths
   - win and lose transitions
   - menu/settings/info navigation
3. Solver stress:
   - large hard board with ensure-solvable enabled
   - repeated relaunch cycles
   - confirm no input/render lockups
4. Shutdown test:
   - exit app during solver activity
   - relaunch and verify clean startup
