• Global Constraints / Tips (Read Before Implementing)
  1.0 [ ] Preserve save compatibility: keep MINESWEEPER_SETTINGS_FILE_VERSION strategy explicit and add
  migration behavior when adding ensure_solvable_board persistence.
  1.1 [ ] Keep app responsive: avoid long/blocking work in GUI thread paths (input callbacks, draw
  callbacks, scene enter/exit).
  1.2 [ ] Guard all new loops with limits/timeouts (especially solvable-board generation).
  1.3 [ ] Keep behavior deterministic where possible; random tie-break logic should be isolated and
  documented.
  1.4 [ ] Validate every state transition with both “new app start” and “return from scene” flows.
  1.5 [ ] After each stage, run build + smoke test (ufbt / on-device quick pass): launch, play, settings
  save, exit, relaunch.
  1.6 [ ] Maintain small commits per stage to simplify rollback and bisect.

  STAGE 1: minesweeper.h + minesweeper.c (Bootstrap / Global State)
  1.0 [ ] Audit and remove/rename dead fields (ensure_map_solvable) and align state ownership
  (settings_info vs t_settings_info).
  1.1 [ ] Ensure defaults initialize all settings fields, including ensure_solvable_board.
  1.2 [ ] Wire game alloc/reset to actual stored ensure_solvable_board (no hardcoded false path).
  1.3 [ ] Add defensive checks for allocation/service open failures and define clear fallback/exit
  behavior.
  1.4 [ ] Verify startup/restart flow: cold start, start screen -> game, settings round-trip, app close/
  reopen.

  STAGE 2: helpers/mine_sweeper_storage.h + helpers/mine_sweeper_storage.c (Persistence Correctness
  First)
  2.0 [x] Add version migration path for older configs missing new key(s).
  2.1 [x] Fix all early-return cleanup leaks (FuriString, FlipperFormat, storage handles).
  2.2 [x] Replace fragile save flow with atomic write (.tmp + rename/swap).
  2.3 [x] Clean ambiguous condition expressions and harden error logging for diagnosability.
  2.4 [x] Validate persistence matrix: missing file, old file version, corrupted fields, normal save/
  load.

  STAGE 3: views/minesweeper_game_screen.h + views/minesweeper_game_screen.c + views/
  minesweeper_game_screen_i.h (Core Gameplay Safety/UX)
  3.0 [ ] Add retry cap/fallback for solvable-board generation loop to prevent potential infinite spin.
  3.1 [ ] Fix end-screen reset input flow to match UX copy (“PRESS OK” should be single clear action).
  3.2 [ ] Fix render formatting/type mismatches (coordinate and status formatting).
  3.3 [ ] Prevent false-positive feedback on no-op actions (e.g., trying to flag with zero flags left).
  3.4 [ ] Review hot-path blocking calls in input/move handlers and reduce latency impact.
  3.5 [ ] Re-verify win/lose invariants (mines_left, flags_left, tiles_left) across all input variants.
  3.6 [ ] Validate with test scenarios: wrap on/off, long-OK chord clear, long-back jump, win via clear,
  win via flagging, lose via click/chord.

  STAGE 4: scenes/settings_scene.c (Headerless Scene: Settings Event Integrity)
  4.0 [x] Fix swapped width/height custom event dispatch constants.
  4.1 [x] Remove unused enums/definitions and tighten event switch logic.
  4.2 [x] Recheck is_settings_changed criteria and confirm it includes intended fields only.
  4.3 [x] Ensure immediate-save settings (feedback, wrap) and deferred-reset settings behave
  consixtently.
  4.4 [x] Validate navigation: settings -> info -> back -> confirmation -> save/cancel/back.

  STAGE 5: scenes/confirmation_scene.c (Headerless Scene: Commit/Cancel Semantics)
  5.0 [ ] Ensure save path commits all intended fields and nothing unintended.
  5.1 [ ] Ensure cancel path preserves active board and clears temp-change state correctly.
  5.2 [ ] Confirm scene switch target behavior is consistent after save/cancel/back.
  5.3 [ ] Add guard/logging around reset+scene switch sequencing for easier debugging.

  STAGE 6: scenes/start_screen_scene.c (Headerless Scene: Input Filtering)
  6.0 [x] Restrict “continue” trigger to intended key/type (not all non-back events).
  6.1 [x] Verify no accidental skip on repeats/releases/noise inputs.
  6.2 [x] Confirm start-screen exit/reset leaves animation/resources clean.

  STAGE 7: scenes/menu_scene.c (Headerless Scene: Event Consumption and Return Paths)
  7.0 [x] Ensure center button (Settings) marks event consumed consistently.
  7.1 [x] Revalidate fallback stop behavior when previous scene search fails.
  7.2 [x] Confirm menu reopen path does not duplicate state or callbacks.

  STAGE 8: scenes/game_screen_scene.c + scenes/info_scene.c (Headerless Scenes: Minor Stability Pass)
  8.0 [x] Confirm game scene enter/exit expectations (no hidden reset side effects).
  8.1 [x] Confirm info scene back/navigation behavior is explicit and stable.
  8.2 [x] Remove unused code/UNUSEDs if no longer necessary after earlier fixes.

  STAGE 9: helpers/mine_sweeper_led.h + helpers/mine_sweeper_led.c (Feedback Refactor: LED)
  9.0 [x] Replace stack-lifetime NotificationMessage sequence construction with safe static/owned
  pattern.
  9.1 [x] Remove timing sleeps used as lifetime workaround.
  9.2 [x] Add context assertions and normalize API input validation.
  9.3 [x] Re-test visual feedback under rapid input repetition.

  STAGE 10: helpers/mine_sweeper_haptic.h + helpers/mine_sweeper_haptic.c (Feedback Refactor: Haptics)
  10.0 [x] Add context assertions and early-return when feedback disabled.
  10.1 [x] Consolidate duplicated bump logic into shared helper(s).
  10.2 [x] Reduce/blocking waits or move to sequence-driven model where possible.
  10.3 [x] Validate haptic patterns for move/oob/wrap/win/lose don’t stall gameplay input.

  STAGE 11: helpers/mine_sweeper_speaker.h + helpers/mine_sweeper_speaker.c (Feedback Cleanup: Sound)
  11.0 [x] Keep existing notification-sequence model; add missing consistency guards/asserts only.
  11.1 [x] Normalize naming/structure to match LED+haptic refactor style.
  11.2 [x] Verify mute/volume respect is preserved after any cleanup.

  STAGE 12: views/start_screen.h + views/start_screen.c (View Hygiene)
  12.0 [x] Add defensive initialization/cleanup consistency for context/callback fields.
  12.1 [x] Recheck animation lifecycle under repeated scene enter/exit.
  12.2 [x] Keep as low-risk cleanup only unless stage testing reveals functional issues.

  STAGE 13: scenes/minesweeper_scene.h + scenes/minesweeper_scene.c (Scene Framework Integrity)
  13.0 [ ] Verify handler arrays match enum count and scene config exactly.
  13.1 [ ] Add lightweight compile-time/runtime sanity checks if feasible.
  13.2 [ ] Final regression pass over full scene graph navigation.

  Final Validation Stage
  14.0 [ ] Full manual regression suite: start, play, lose, win, restart, menu, settings save/cancel,
  info screen, wrap toggle, feedback toggle, ensure-solvable on/off, app relaunch persistence.
  14.1 [ ] Stress scenarios: max board size, rapid directional repeat, repeated restarts, repeated
  settings toggles.
  14.2 [ ] Final cleanup pass for logs/comments/unused symbols and prep for implementation commits.
